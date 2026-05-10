`timescale 1ns / 1ps

// gelu_unit — Pipelined synthesisable GELU(x) using Padé [3/2] tanh.
//
// GELU(x) = 0.5 · x · (1 + tanh(√(2/π) · (x + 0.044715·x³)))
// tanh  ≈ arg·(1485 + 171·arg² + 2·arg⁴) / (1485 + 666·arg² + 26·arg⁴)
// 1/den via bit-trick initial estimate + 2 Newton-Raphson iterations.
//
// Pipeline: 5 register banks (R0..R4) + combinational output stage.
// Latency:  5 clock cycles from input to valid result.
// Each pipeline interval contains ≤ 3 sequential bf16_mul levels (~1350 ps),
// well within the 1650 ps clock target without requiring synthesis retiming.
//
// Stage depths (bf16_mul levels between registers):
//   Input→R0 : x², x³                          (2 muls)
//   R0→R1    : cube_term, inner, arg, arg²      (3 muls)
//   R1→R2    : arg⁴, polynomials, num, den      (3 muls)
//   R2→R3    : NR iteration 1 → y1             (2 muls)
//   R3→R4    : NR iteration 2, tanh_raw, half_x (3 muls)
//   R4→out   : tanh_val, 1+tanh, result         (1 mul + add ≈ 800 ps)
//
// Note: the 5-cycle latency means the first 5 beats of each readback burst
// carry zeros (pipeline filling from reset state).  For M_count=256 this
// is 5 of 512 beats — handled in axi_readback with a warmup counter.

module gelu_unit (
    input  logic clk,
    input  logic rst,

    input  logic [31:0] x,       // FP32 input
    output logic [31:0] result   // FP32 GELU(x), valid 5 cycles after x
);
    // ── Combinational: Input → R0 ─────────────────────────────────────────────
    // x², x³  (2 sequential muls)
    logic [31:0] c0_x2, c0_x3;
    logic [15:0] x_bf16;
    assign x_bf16 = x[31:16];

    bf16_mul u_x2 (.act_bf16(x_bf16),        .wt_bf16(x_bf16),        .product_fp32(c0_x2));
    bf16_mul u_x3 (.act_bf16(x_bf16),        .wt_bf16(c0_x2[31:16]),  .product_fp32(c0_x3));

    // ── R0 ───────────────────────────────────────────────────────────────────
    logic [31:0] r0_x2, r0_x3, r0_x;
    logic [15:0] r0_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r0_x2<='0; r0_x3<='0; r0_x<='0; r0_xb<='0; end
        else     begin r0_x2<=c0_x2; r0_x3<=c0_x3; r0_x<=x; r0_xb<=x_bf16; end
    end

    // ── Combinational: R0 → R1 ────────────────────────────────────────────────
    // cube_term (1), inner (add), arg (2), arg² (3)  — 3 sequential muls
    logic [31:0] c1_cube, c1_inner, c1_arg, c1_arg2;
    logic        c1_sat;

    bf16_mul      u_ct  (.act_bf16(16'h3D37),        .wt_bf16(r0_x3[31:16]),  .product_fp32(c1_cube));
    bf16_fp32_add u_in  (.a(c1_cube),                .b(r0_x),                .result(c1_inner));
    bf16_mul      u_arg (.act_bf16(16'h3F4C),        .wt_bf16(c1_inner[31:16]),.product_fp32(c1_arg));
    bf16_mul      u_a2  (.act_bf16(c1_arg[31:16]),   .wt_bf16(c1_arg[31:16]), .product_fp32(c1_arg2));

    assign c1_sat = ({1'b0, c1_arg[30:0]} >= 32'h40800000);

    // ── R1 ───────────────────────────────────────────────────────────────────
    logic [31:0] r1_arg, r1_arg2;
    logic        r1_sat, r1_asign;
    logic [15:0] r1_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r1_arg<='0; r1_arg2<='0; r1_sat<='0; r1_asign<='0; r1_xb<='0; end
        else     begin r1_arg<=c1_arg; r1_arg2<=c1_arg2; r1_sat<=c1_sat;
                       r1_asign<=c1_arg[31]; r1_xb<=r0_xb; end
    end

    // ── Combinational: R1 → R2 ────────────────────────────────────────────────
    // arg⁴ (1), 171·arg² / 666·arg² (1 each, parallel),
    // 2·arg⁴ / 26·arg⁴ (2 each, via arg⁴),
    // num_inner / den (adds), num = arg × num_inner (3 muls via arg⁴ chain)
    logic [31:0] c2_arg4, c2_a171, c2_a2x, c2_a666, c2_a26;
    logic [31:0] c2_tnum, c2_ninner, c2_tden, c2_den, c2_num;

    bf16_mul      u_a4  (.act_bf16(r1_arg2[31:16]), .wt_bf16(r1_arg2[31:16]), .product_fp32(c2_arg4));
    bf16_mul      u_171 (.act_bf16(16'h432B),       .wt_bf16(r1_arg2[31:16]), .product_fp32(c2_a171));
    bf16_mul      u_666 (.act_bf16(16'h4426),       .wt_bf16(r1_arg2[31:16]), .product_fp32(c2_a666));
    bf16_mul      u_2x  (.act_bf16(16'h4000),       .wt_bf16(c2_arg4[31:16]), .product_fp32(c2_a2x));
    bf16_mul      u_26  (.act_bf16(16'h41D0),       .wt_bf16(c2_arg4[31:16]), .product_fp32(c2_a26));
    bf16_fp32_add u_t1  (.a(32'h44B9A000), .b(c2_a171), .result(c2_tnum));
    bf16_fp32_add u_ni  (.a(c2_tnum),      .b(c2_a2x),  .result(c2_ninner));
    bf16_fp32_add u_t2  (.a(32'h44B9A000), .b(c2_a666), .result(c2_tden));
    bf16_fp32_add u_dn  (.a(c2_tden),      .b(c2_a26),  .result(c2_den));
    bf16_mul      u_nm  (.act_bf16(r1_arg[31:16]),  .wt_bf16(c2_ninner[31:16]), .product_fp32(c2_num));

    // ── R2 ───────────────────────────────────────────────────────────────────
    logic [31:0] r2_den, r2_num;
    logic        r2_sat, r2_asign;
    logic [15:0] r2_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r2_den<='0; r2_num<='0; r2_sat<='0; r2_asign<='0; r2_xb<='0; end
        else     begin r2_den<=c2_den; r2_num<=c2_num; r2_sat<=r1_sat;
                       r2_asign<=r1_asign; r2_xb<=r1_xb; end
    end

    // ── Combinational: R2 → R3 ────────────────────────────────────────────────
    // NR iteration 1: y0 (bit trick, free), xy0 (1 mul), err0 (negate+add), y1 (2 muls)
    logic [31:0] c3_y0, c3_xy0, c3_neg_xy0, c3_err0, c3_y1;

    assign c3_y0      = 32'h7EEEEEEE - {1'b0, r2_den[30:0]};
    bf16_mul      u_m0 (.act_bf16(r2_den[31:16]), .wt_bf16(c3_y0[31:16]),   .product_fp32(c3_xy0));
    assign c3_neg_xy0 = {~c3_xy0[31], c3_xy0[30:0]};
    bf16_fp32_add u_a0 (.a(c3_neg_xy0), .b(32'h40000000), .result(c3_err0));
    bf16_mul      u_m1 (.act_bf16(c3_y0[31:16]),  .wt_bf16(c3_err0[31:16]), .product_fp32(c3_y1));

    // ── R3 ───────────────────────────────────────────────────────────────────
    logic [31:0] r3_y1, r3_den, r3_num;
    logic        r3_sat, r3_asign;
    logic [15:0] r3_xb;
    always_ff @(posedge clk) begin
        if (rst) begin r3_y1<='0; r3_den<='0; r3_num<='0; r3_sat<='0; r3_asign<='0; r3_xb<='0; end
        else     begin r3_y1<=c3_y1; r3_den<=r2_den; r3_num<=r2_num; r3_sat<=r2_sat;
                       r3_asign<=r2_asign; r3_xb<=r2_xb; end
    end

    // ── Combinational: R3 → R4 ────────────────────────────────────────────────
    // NR iteration 2: xy1 (1 mul), err1 (negate+add), y2 (2 muls)
    // tanh_raw = num × y2  (3 muls via y2 chain)
    // half_x = 0.5 × x    (1 mul, parallel)
    logic [31:0] c4_xy1, c4_neg_xy1, c4_err1, c4_y2;
    logic [31:0] c4_tanh_raw, c4_half_x;

    bf16_mul      u_m2  (.act_bf16(r3_den[31:16]),  .wt_bf16(r3_y1[31:16]),   .product_fp32(c4_xy1));
    assign c4_neg_xy1   = {~c4_xy1[31], c4_xy1[30:0]};
    bf16_fp32_add u_a1  (.a(c4_neg_xy1), .b(32'h40000000), .result(c4_err1));
    bf16_mul      u_m3  (.act_bf16(r3_y1[31:16]),   .wt_bf16(c4_err1[31:16]), .product_fp32(c4_y2));
    bf16_mul      u_tr  (.act_bf16(r3_num[31:16]),  .wt_bf16(c4_y2[31:16]),   .product_fp32(c4_tanh_raw));
    bf16_mul      u_hx  (.act_bf16(16'h3F00),       .wt_bf16(r3_xb),          .product_fp32(c4_half_x));

    // ── R4 ───────────────────────────────────────────────────────────────────
    logic [31:0] r4_tanh_raw, r4_half_x;
    logic        r4_sat, r4_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r4_tanh_raw<='0; r4_half_x<='0; r4_sat<='0; r4_asign<='0; end
        else     begin r4_tanh_raw<=c4_tanh_raw; r4_half_x<=c4_half_x;
                       r4_sat<=r3_sat; r4_asign<=r3_asign; end
    end

    // ── Combinational: R4 → result ────────────────────────────────────────────
    // tanh_val (mux), 1+tanh (add), result = half_x × one_plus_tanh (1 mul)
    // Critical path: add + 1 mul ≈ 800 ps
    logic [31:0] tanh_val, one_plus_tanh;

    assign tanh_val = r4_sat ? {r4_asign, 8'h7F, 23'h0} : r4_tanh_raw;
    bf16_fp32_add u_1t (.a(32'h3F800000), .b(tanh_val),            .result(one_plus_tanh));
    bf16_mul      u_rs (.act_bf16(r4_half_x[31:16]),
                        .wt_bf16(one_plus_tanh[31:16]),
                        .product_fp32(result));

endmodule
