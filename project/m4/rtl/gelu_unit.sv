`timescale 1ns / 1ps

// gelu_unit — Pipelined GELU(x) with Padé [3/2] tanh, max 2 bf16_mul per stage.
//
// GELU(x) = 0.5 · x · (1 + tanh(√(2/π) · (x + 0.044715·x³)))
// tanh  ≈ arg·(1485 + 171·arg² + 2·arg⁴) / (1485 + 666·arg² + 26·arg⁴)
// 1/den via bit-trick initial estimate + 2 Newton-Raphson iterations.
//
// Pipeline: 8 register banks (R0, R0b, R1, R1b, R2, R3, R3b, R4)
//           plus a final combinational output stage.
// Latency:  8 clock cycles.
//
// Each pipeline interval contains at most 2 SEQUENTIAL bf16_mul levels.
// At ~450 ps per mul plus routing overhead: ~1100-1200 ps per stage,
// comfortably within a 1650 ps clock target.
//
// Stage-by-stage breakdown (sequential mul depth → approx path):
//   Input → R0  : x², x³                            2 muls  ~900 ps
//   R0    → R0b : cube_term, inner(add), arg         2 muls  ~900 ps
//   R0b   → R1  : arg²                               1 mul   ~450 ps
//   R1    → R1b : arg⁴, a171, a666   (all parallel) 1 mul   ~450 ps
//   R1b   → R2  : a2x(1)→num_inner(add)→num(2)      2 muls  ~900 ps
//   R2    → R3  : y0(free), xy0(1)→y1(2)            2 muls  ~900 ps
//   R3    → R3b : xy1(1)→y2(2)                       2 muls  ~900 ps
//   R3b   → R4  : tanh_raw, half_x   (parallel)      1 mul   ~450 ps
//   R4    → out : tanh_val(mux), 1+tanh(add), result  1 mul   ~800 ps

module gelu_unit (
    input  logic clk,
    input  logic rst,

    input  logic [31:0] x,       // FP32 input
    output logic [31:0] result   // FP32 GELU(x), valid 8 cycles after x
);
    logic [15:0] x_bf16;
    assign x_bf16 = x[31:16];

    // =========================================================================
    // Input → R0  (x², x³ — 2 sequential muls)
    // =========================================================================
    logic [31:0] c0_x2, c0_x3;
    bf16_mul u_x2 (.act_bf16(x_bf16),       .wt_bf16(x_bf16),       .product_fp32(c0_x2));
    bf16_mul u_x3 (.act_bf16(x_bf16),       .wt_bf16(c0_x2[31:16]), .product_fp32(c0_x3));

    logic [31:0] r0_x2, r0_x3, r0_x;
    logic [15:0] r0_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r0_x2<='0; r0_x3<='0; r0_x<='0; r0_xb<='0; end
        else     begin r0_x2<=c0_x2; r0_x3<=c0_x3; r0_x<=x; r0_xb<=x_bf16; end
    end

    // =========================================================================
    // R0 → R0b  (cube_term, inner(add), arg — 2 sequential muls)
    // =========================================================================
    logic [31:0] c0b_cube, c0b_inner, c0b_arg;
    bf16_mul      u_ct  (.act_bf16(16'h3D37),     .wt_bf16(r0_x3[31:16]),  .product_fp32(c0b_cube));
    bf16_fp32_add u_in  (.a(c0b_cube),            .b(r0_x),                .result(c0b_inner));
    bf16_mul      u_arg (.act_bf16(16'h3F4C),     .wt_bf16(c0b_inner[31:16]), .product_fp32(c0b_arg));

    logic [31:0] r0b_arg, r0b_x;
    logic [15:0] r0b_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r0b_arg<='0; r0b_x<='0; r0b_xb<='0; end
        else     begin r0b_arg<=c0b_arg; r0b_x<=r0_x; r0b_xb<=r0_xb; end
    end

    // =========================================================================
    // R0b → R1  (arg² — 1 mul; also latch sat, arg_sign, bypasses)
    // =========================================================================
    logic [31:0] c1_arg2;
    logic        c1_sat;
    bf16_mul u_a2 (.act_bf16(r0b_arg[31:16]), .wt_bf16(r0b_arg[31:16]), .product_fp32(c1_arg2));
    assign c1_sat = ({1'b0, r0b_arg[30:0]} >= 32'h40800000);

    logic [31:0] r1_arg2, r1_arg;
    logic        r1_sat, r1_asign;
    logic [15:0] r1_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r1_arg2<='0; r1_arg<='0; r1_sat<='0; r1_asign<='0; r1_xb<='0; end
        else     begin r1_arg2<=c1_arg2; r1_arg<=r0b_arg; r1_sat<=c1_sat;
                       r1_asign<=r0b_arg[31]; r1_xb<=r0b_xb; end
    end

    // =========================================================================
    // R1 → R1b  (arg⁴, a171, a666 — all parallel, 1 mul level)
    // =========================================================================
    logic [31:0] c1b_arg4, c1b_a171, c1b_a666;
    bf16_mul u_a4  (.act_bf16(r1_arg2[31:16]), .wt_bf16(r1_arg2[31:16]), .product_fp32(c1b_arg4));
    bf16_mul u_171 (.act_bf16(16'h432B),       .wt_bf16(r1_arg2[31:16]), .product_fp32(c1b_a171));
    bf16_mul u_666 (.act_bf16(16'h4426),       .wt_bf16(r1_arg2[31:16]), .product_fp32(c1b_a666));

    logic [31:0] r1b_arg4, r1b_a171, r1b_a666, r1b_arg;
    logic        r1b_sat, r1b_asign;
    logic [15:0] r1b_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r1b_arg4<='0; r1b_a171<='0; r1b_a666<='0;
                       r1b_arg<='0; r1b_sat<='0; r1b_asign<='0; r1b_xb<='0; end
        else     begin r1b_arg4<=c1b_arg4; r1b_a171<=c1b_a171; r1b_a666<=c1b_a666;
                       r1b_arg<=r1_arg; r1b_sat<=r1_sat;
                       r1b_asign<=r1_asign; r1b_xb<=r1_xb; end
    end

    // =========================================================================
    // R1b → R2  (a2x(1)→num_inner(add)→num(2); a26 parallel — 2 sequential muls)
    // =========================================================================
    logic [31:0] c2_a2x, c2_a26;
    logic [31:0] c2_tnum, c2_ninner, c2_tden, c2_den, c2_num;
    bf16_mul      u_2x  (.act_bf16(16'h4000), .wt_bf16(r1b_arg4[31:16]), .product_fp32(c2_a2x));
    bf16_mul      u_26  (.act_bf16(16'h41D0), .wt_bf16(r1b_arg4[31:16]), .product_fp32(c2_a26));
    bf16_fp32_add u_t1  (.a(32'h44B9A000), .b(r1b_a171),   .result(c2_tnum));
    bf16_fp32_add u_ni  (.a(c2_tnum),      .b(c2_a2x),     .result(c2_ninner));
    bf16_fp32_add u_t2  (.a(32'h44B9A000), .b(r1b_a666),   .result(c2_tden));
    bf16_fp32_add u_dn  (.a(c2_tden),      .b(c2_a26),     .result(c2_den));
    bf16_mul      u_nm  (.act_bf16(r1b_arg[31:16]), .wt_bf16(c2_ninner[31:16]), .product_fp32(c2_num));

    logic [31:0] r2_den, r2_num;
    logic        r2_sat, r2_asign;
    logic [15:0] r2_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r2_den<='0; r2_num<='0; r2_sat<='0; r2_asign<='0; r2_xb<='0; end
        else     begin r2_den<=c2_den; r2_num<=c2_num; r2_sat<=r1b_sat;
                       r2_asign<=r1b_asign; r2_xb<=r1b_xb; end
    end

    // =========================================================================
    // R2 → R3  (NR iter 1: y0(free), xy0(1)→err0(add)→y1(2) — 2 sequential muls)
    // =========================================================================
    logic [31:0] c3_y0, c3_xy0, c3_neg_xy0, c3_err0, c3_y1;
    assign c3_y0      = 32'h7EEEEEEE - {1'b0, r2_den[30:0]};
    bf16_mul      u_m0 (.act_bf16(r2_den[31:16]), .wt_bf16(c3_y0[31:16]),   .product_fp32(c3_xy0));
    assign c3_neg_xy0 = {~c3_xy0[31], c3_xy0[30:0]};
    bf16_fp32_add u_a0 (.a(c3_neg_xy0), .b(32'h40000000), .result(c3_err0));
    bf16_mul      u_m1 (.act_bf16(c3_y0[31:16]), .wt_bf16(c3_err0[31:16]),  .product_fp32(c3_y1));

    logic [31:0] r3_y1, r3_den, r3_num;
    logic        r3_sat, r3_asign;
    logic [15:0] r3_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r3_y1<='0; r3_den<='0; r3_num<='0; r3_sat<='0; r3_asign<='0; r3_xb<='0; end
        else     begin r3_y1<=c3_y1; r3_den<=r2_den; r3_num<=r2_num; r3_sat<=r2_sat;
                       r3_asign<=r2_asign; r3_xb<=r2_xb; end
    end

    // =========================================================================
    // R3 → R3b  (NR iter 2 first: xy1(1)→err1(add)→y2(2) — 2 sequential muls)
    // =========================================================================
    logic [31:0] c3b_xy1, c3b_neg_xy1, c3b_err1, c3b_y2;
    bf16_mul      u_m2 (.act_bf16(r3_den[31:16]), .wt_bf16(r3_y1[31:16]),   .product_fp32(c3b_xy1));
    assign c3b_neg_xy1 = {~c3b_xy1[31], c3b_xy1[30:0]};
    bf16_fp32_add u_a1 (.a(c3b_neg_xy1), .b(32'h40000000), .result(c3b_err1));
    bf16_mul      u_m3 (.act_bf16(r3_y1[31:16]), .wt_bf16(c3b_err1[31:16]), .product_fp32(c3b_y2));

    logic [31:0] r3b_y2, r3b_num;
    logic        r3b_sat, r3b_asign;
    logic [15:0] r3b_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r3b_y2<='0; r3b_num<='0; r3b_sat<='0; r3b_asign<='0; r3b_xb<='0; end
        else     begin r3b_y2<=c3b_y2; r3b_num<=r3_num; r3b_sat<=r3_sat;
                       r3b_asign<=r3_asign; r3b_xb<=r3_xb; end
    end

    // =========================================================================
    // R3b → R4  (tanh_raw, half_x — parallel, 1 mul level each)
    // =========================================================================
    logic [31:0] c4_tanh_raw, c4_half_x;
    bf16_mul u_tr (.act_bf16(r3b_num[31:16]), .wt_bf16(r3b_y2[31:16]),  .product_fp32(c4_tanh_raw));
    bf16_mul u_hx (.act_bf16(16'h3F00),       .wt_bf16(r3b_xb),         .product_fp32(c4_half_x));

    logic [31:0] r4_tanh_raw, r4_half_x;
    logic        r4_sat, r4_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r4_tanh_raw<='0; r4_half_x<='0; r4_sat<='0; r4_asign<='0; end
        else     begin r4_tanh_raw<=c4_tanh_raw; r4_half_x<=c4_half_x;
                       r4_sat<=r3b_sat; r4_asign<=r3b_asign; end
    end

    // =========================================================================
    // R4 → output  (tanh_val mux, 1+tanh add, result — 1 mul ~800 ps)
    // =========================================================================
    logic [31:0] tanh_val, one_plus_tanh;
    assign tanh_val = r4_sat ? {r4_asign, 8'h7F, 23'h0} : r4_tanh_raw;
    bf16_fp32_add u_1t (.a(32'h3F800000), .b(tanh_val),             .result(one_plus_tanh));
    bf16_mul      u_rs (.act_bf16(r4_half_x[31:16]),
                        .wt_bf16(one_plus_tanh[31:16]),
                        .product_fp32(result));

endmodule
