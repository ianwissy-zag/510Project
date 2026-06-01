`timescale 1ns / 1ps

// gelu_unit — Pipelined GELU(x) with Padé [3/2] tanh, one FP32 op per stage.
//
// GELU(x) = 0.5 · x · (1 + tanh(√(2/π) · (x + 0.044715·x³)))
// tanh  ≈ arg · num_inner / den
//   num_inner = 1485 + 171·arg² + 2·arg⁴
//   den       = 1485 + 666·arg² + 26·arg⁴
// 1/den via bit-trick initial estimate + 2 Newton-Raphson iterations.
//
// Pipeline: 20 register banks (R1..R20), one FP32 operation per stage.
// Latency: 20 clock cycles.
//
// Stage → Operation(s):
//   S1:  x2    = x * x
//   S2:  x3    = x * x2
//   S3:  cube  = 0.044715 * x3
//   S4:  inner = cube + x                             [ADD]
//   S5:  arg   = 0.7978845608 * inner
//   S6:  arg2  = arg * arg
//   S7:  arg4=arg2*arg2, a171=171*arg2, a666=666*arg2 [3× parallel MUL]
//   S8:  a2=2*arg4, a26=26*arg4                       [2× parallel MUL]
//   S9:  tnum=1485+a171, tden=1485+a666               [2× parallel ADD]
//   S10: num_inner=tnum+a2, den=tden+a26              [2× parallel ADD]
//   S11: num    = arg * num_inner
//   S12: xy0   = den * y0   (y0 = bit_trick(den), free)
//   S13: err0  = 2.0 - xy0                            [ADD; neg is free]
//   S14: y1    = y0 * err0
//   S15: xy1   = den * y1
//   S16: err1  = 2.0 - xy1                            [ADD; neg is free]
//   S17: y2    = y1 * err1
//   S18: tanh_raw=num*y2, half_x=0.5*x               [2× parallel MUL]
//   S19: one_plus_tanh = 1.0 + tanh_val               [ADD; sat mux is free]
//   S20: result = half_x * one_plus_tanh

module gelu_unit (
    input  logic clk,
    input  logic rst,

    input  logic [31:0] x,       // FP32 input
    output logic [31:0] result   // FP32 GELU(x), valid 20 cycles after x
);
    // FP32 constants
    localparam logic [31:0] C_044715  = 32'h3D37D9AC; // 0.044715
    localparam logic [31:0] C_SQRT2PI = 32'h3F4C422A; // sqrt(2/pi) ≈ 0.7978845608
    localparam logic [31:0] C_171     = 32'h432B0000; // 171.0
    localparam logic [31:0] C_666     = 32'h44268000; // 666.0
    localparam logic [31:0] C_2       = 32'h40000000; // 2.0
    localparam logic [31:0] C_26      = 32'h41D00000; // 26.0
    localparam logic [31:0] C_1485    = 32'h44B9A000; // 1485.0
    localparam logic [31:0] C_1       = 32'h3F800000; // 1.0
    localparam logic [31:0] C_05      = 32'h3F000000; // 0.5

    // =========================================================================
    // Stage 1: x2 = x * x
    // =========================================================================
    logic [31:0] c1_x2;
    fp32_mul u_s1 (.a(x), .b(x), .result(c1_x2));

    logic [31:0] r1_x, r1_x2;
    always_ff @(posedge clk) begin
        if (rst) begin r1_x <= '0; r1_x2 <= '0; end
        else     begin r1_x <= x;  r1_x2 <= c1_x2; end
    end

    // =========================================================================
    // Stage 2: x3 = x * x2
    // =========================================================================
    logic [31:0] c2_x3;
    fp32_mul u_s2 (.a(r1_x), .b(r1_x2), .result(c2_x3));

    logic [31:0] r2_x, r2_x3;
    always_ff @(posedge clk) begin
        if (rst) begin r2_x <= '0; r2_x3 <= '0; end
        else     begin r2_x <= r1_x; r2_x3 <= c2_x3; end
    end

    // =========================================================================
    // Stage 3: cube = 0.044715 * x3
    // =========================================================================
    logic [31:0] c3_cube;
    fp32_mul u_s3 (.a(C_044715), .b(r2_x3), .result(c3_cube));

    logic [31:0] r3_x, r3_cube;
    always_ff @(posedge clk) begin
        if (rst) begin r3_x <= '0; r3_cube <= '0; end
        else     begin r3_x <= r2_x; r3_cube <= c3_cube; end
    end

    // =========================================================================
    // Stage 4: inner = cube + x  [ADD]
    // =========================================================================
    logic [31:0] c4_inner;
    bf16_fp32_add u_s4 (.a(r3_cube), .b(r3_x), .result(c4_inner));

    logic [31:0] r4_x, r4_inner;
    always_ff @(posedge clk) begin
        if (rst) begin r4_x <= '0; r4_inner <= '0; end
        else     begin r4_x <= r3_x; r4_inner <= c4_inner; end
    end

    // =========================================================================
    // Stage 5: arg = SQRT2PI * inner
    //          sat and asign captured from combinational arg
    // =========================================================================
    logic [31:0] c5_arg;
    fp32_mul u_s5 (.a(C_SQRT2PI), .b(r4_inner), .result(c5_arg));

    logic c5_sat, c5_asign;
    assign c5_sat   = ({1'b0, c5_arg[30:0]} >= 32'h40800000); // |arg| >= 4.0
    assign c5_asign = c5_arg[31];

    logic [31:0] r5_x, r5_arg;
    logic        r5_sat, r5_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r5_x <= '0; r5_arg <= '0; r5_sat <= '0; r5_asign <= '0; end
        else     begin r5_x <= r4_x; r5_arg <= c5_arg; r5_sat <= c5_sat; r5_asign <= c5_asign; end
    end

    // =========================================================================
    // Stage 6: arg2 = arg * arg
    // =========================================================================
    logic [31:0] c6_arg2;
    fp32_mul u_s6 (.a(r5_arg), .b(r5_arg), .result(c6_arg2));

    logic [31:0] r6_x, r6_arg, r6_arg2;
    logic        r6_sat, r6_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r6_x <= '0; r6_arg <= '0; r6_arg2 <= '0; r6_sat <= '0; r6_asign <= '0; end
        else     begin r6_x <= r5_x; r6_arg <= r5_arg; r6_arg2 <= c6_arg2;
                       r6_sat <= r5_sat; r6_asign <= r5_asign; end
    end

    // =========================================================================
    // Stage 7: arg4=arg2*arg2, a171=171*arg2, a666=666*arg2  [3× parallel MUL]
    // =========================================================================
    logic [31:0] c7_arg4, c7_a171, c7_a666;
    fp32_mul u_s7_a4  (.a(r6_arg2), .b(r6_arg2), .result(c7_arg4));
    fp32_mul u_s7_171 (.a(C_171),   .b(r6_arg2), .result(c7_a171));
    fp32_mul u_s7_666 (.a(C_666),   .b(r6_arg2), .result(c7_a666));

    logic [31:0] r7_x, r7_arg, r7_arg4, r7_a171, r7_a666;
    logic        r7_sat, r7_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r7_x <= '0; r7_arg <= '0; r7_arg4 <= '0;
                       r7_a171 <= '0; r7_a666 <= '0; r7_sat <= '0; r7_asign <= '0; end
        else     begin r7_x <= r6_x; r7_arg <= r6_arg; r7_arg4 <= c7_arg4;
                       r7_a171 <= c7_a171; r7_a666 <= c7_a666;
                       r7_sat <= r6_sat; r7_asign <= r6_asign; end
    end

    // =========================================================================
    // Stage 8: a2=2*arg4, a26=26*arg4  [2× parallel MUL]
    // =========================================================================
    logic [31:0] c8_a2, c8_a26;
    fp32_mul u_s8_2   (.a(C_2),  .b(r7_arg4), .result(c8_a2));
    fp32_mul u_s8_26  (.a(C_26), .b(r7_arg4), .result(c8_a26));

    logic [31:0] r8_x, r8_arg, r8_a2, r8_a26, r8_a171, r8_a666;
    logic        r8_sat, r8_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r8_x <= '0; r8_arg <= '0; r8_a2 <= '0; r8_a26 <= '0;
                       r8_a171 <= '0; r8_a666 <= '0; r8_sat <= '0; r8_asign <= '0; end
        else     begin r8_x <= r7_x; r8_arg <= r7_arg; r8_a2 <= c8_a2; r8_a26 <= c8_a26;
                       r8_a171 <= r7_a171; r8_a666 <= r7_a666;
                       r8_sat <= r7_sat; r8_asign <= r7_asign; end
    end

    // =========================================================================
    // Stage 9: tnum=1485+a171, tden=1485+a666  [2× parallel ADD]
    // =========================================================================
    logic [31:0] c9_tnum, c9_tden;
    bf16_fp32_add u_s9_tn (.a(C_1485), .b(r8_a171), .result(c9_tnum));
    bf16_fp32_add u_s9_td (.a(C_1485), .b(r8_a666), .result(c9_tden));

    logic [31:0] r9_x, r9_arg, r9_a2, r9_a26, r9_tnum, r9_tden;
    logic        r9_sat, r9_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r9_x <= '0; r9_arg <= '0; r9_a2 <= '0; r9_a26 <= '0;
                       r9_tnum <= '0; r9_tden <= '0; r9_sat <= '0; r9_asign <= '0; end
        else     begin r9_x <= r8_x; r9_arg <= r8_arg; r9_a2 <= r8_a2; r9_a26 <= r8_a26;
                       r9_tnum <= c9_tnum; r9_tden <= c9_tden;
                       r9_sat <= r8_sat; r9_asign <= r8_asign; end
    end

    // =========================================================================
    // Stage 10: num_inner=tnum+a2, den=tden+a26  [2× parallel ADD]
    // =========================================================================
    logic [31:0] c10_ninner, c10_den;
    bf16_fp32_add u_s10_ni  (.a(r9_tnum), .b(r9_a2),  .result(c10_ninner));
    bf16_fp32_add u_s10_den (.a(r9_tden), .b(r9_a26), .result(c10_den));

    logic [31:0] r10_x, r10_arg, r10_ninner, r10_den;
    logic        r10_sat, r10_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r10_x <= '0; r10_arg <= '0; r10_ninner <= '0; r10_den <= '0;
                       r10_sat <= '0; r10_asign <= '0; end
        else     begin r10_x <= r9_x; r10_arg <= r9_arg; r10_ninner <= c10_ninner;
                       r10_den <= c10_den; r10_sat <= r9_sat; r10_asign <= r9_asign; end
    end

    // =========================================================================
    // Stage 11: num = arg * num_inner
    // =========================================================================
    logic [31:0] c11_num;
    fp32_mul u_s11 (.a(r10_arg), .b(r10_ninner), .result(c11_num));

    logic [31:0] r11_x, r11_num, r11_den;
    logic        r11_sat, r11_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r11_x <= '0; r11_num <= '0; r11_den <= '0;
                       r11_sat <= '0; r11_asign <= '0; end
        else     begin r11_x <= r10_x; r11_num <= c11_num; r11_den <= r10_den;
                       r11_sat <= r10_sat; r11_asign <= r10_asign; end
    end

    // =========================================================================
    // Stage 12: xy0 = den * y0   (y0 = bit-trick estimate, free)
    // =========================================================================
    logic [31:0] c12_y0, c12_xy0;
    assign c12_y0  = 32'h7EEEEEEE - {1'b0, r11_den[30:0]};
    fp32_mul u_s12 (.a(r11_den), .b(c12_y0), .result(c12_xy0));

    logic [31:0] r12_x, r12_num, r12_den, r12_y0, r12_xy0;
    logic        r12_sat, r12_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r12_x <= '0; r12_num <= '0; r12_den <= '0;
                       r12_y0 <= '0; r12_xy0 <= '0; r12_sat <= '0; r12_asign <= '0; end
        else     begin r12_x <= r11_x; r12_num <= r11_num; r12_den <= r11_den;
                       r12_y0 <= c12_y0; r12_xy0 <= c12_xy0;
                       r12_sat <= r11_sat; r12_asign <= r11_asign; end
    end

    // =========================================================================
    // Stage 13: err0 = 2.0 - xy0  [ADD; negation of xy0 is a free sign flip]
    // =========================================================================
    logic [31:0] c13_neg_xy0, c13_err0;
    assign c13_neg_xy0 = {~r12_xy0[31], r12_xy0[30:0]};
    bf16_fp32_add u_s13 (.a(c13_neg_xy0), .b(C_2), .result(c13_err0));

    logic [31:0] r13_x, r13_num, r13_den, r13_y0, r13_err0;
    logic        r13_sat, r13_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r13_x <= '0; r13_num <= '0; r13_den <= '0;
                       r13_y0 <= '0; r13_err0 <= '0; r13_sat <= '0; r13_asign <= '0; end
        else     begin r13_x <= r12_x; r13_num <= r12_num; r13_den <= r12_den;
                       r13_y0 <= r12_y0; r13_err0 <= c13_err0;
                       r13_sat <= r12_sat; r13_asign <= r12_asign; end
    end

    // =========================================================================
    // Stage 14: y1 = y0 * err0
    // =========================================================================
    logic [31:0] c14_y1;
    fp32_mul u_s14 (.a(r13_y0), .b(r13_err0), .result(c14_y1));

    logic [31:0] r14_x, r14_num, r14_den, r14_y1;
    logic        r14_sat, r14_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r14_x <= '0; r14_num <= '0; r14_den <= '0;
                       r14_y1 <= '0; r14_sat <= '0; r14_asign <= '0; end
        else     begin r14_x <= r13_x; r14_num <= r13_num; r14_den <= r13_den;
                       r14_y1 <= c14_y1; r14_sat <= r13_sat; r14_asign <= r13_asign; end
    end

    // =========================================================================
    // Stage 15: xy1 = den * y1
    // =========================================================================
    logic [31:0] c15_xy1;
    fp32_mul u_s15 (.a(r14_den), .b(r14_y1), .result(c15_xy1));

    logic [31:0] r15_x, r15_num, r15_y1, r15_xy1;
    logic        r15_sat, r15_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r15_x <= '0; r15_num <= '0; r15_y1 <= '0;
                       r15_xy1 <= '0; r15_sat <= '0; r15_asign <= '0; end
        else     begin r15_x <= r14_x; r15_num <= r14_num; r15_y1 <= r14_y1;
                       r15_xy1 <= c15_xy1; r15_sat <= r14_sat; r15_asign <= r14_asign; end
    end

    // =========================================================================
    // Stage 16: err1 = 2.0 - xy1  [ADD; negation is free]
    // =========================================================================
    logic [31:0] c16_neg_xy1, c16_err1;
    assign c16_neg_xy1 = {~r15_xy1[31], r15_xy1[30:0]};
    bf16_fp32_add u_s16 (.a(c16_neg_xy1), .b(C_2), .result(c16_err1));

    logic [31:0] r16_x, r16_num, r16_y1, r16_err1;
    logic        r16_sat, r16_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r16_x <= '0; r16_num <= '0; r16_y1 <= '0;
                       r16_err1 <= '0; r16_sat <= '0; r16_asign <= '0; end
        else     begin r16_x <= r15_x; r16_num <= r15_num; r16_y1 <= r15_y1;
                       r16_err1 <= c16_err1; r16_sat <= r15_sat; r16_asign <= r15_asign; end
    end

    // =========================================================================
    // Stage 17: y2 = y1 * err1
    // =========================================================================
    logic [31:0] c17_y2;
    fp32_mul u_s17 (.a(r16_y1), .b(r16_err1), .result(c17_y2));

    logic [31:0] r17_x, r17_num, r17_y2;
    logic        r17_sat, r17_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r17_x <= '0; r17_num <= '0; r17_y2 <= '0;
                       r17_sat <= '0; r17_asign <= '0; end
        else     begin r17_x <= r16_x; r17_num <= r16_num; r17_y2 <= c17_y2;
                       r17_sat <= r16_sat; r17_asign <= r16_asign; end
    end

    // =========================================================================
    // Stage 18: tanh_raw=num*y2, half_x=0.5*x  [2× parallel MUL]
    // =========================================================================
    logic [31:0] c18_tanh_raw, c18_half_x;
    fp32_mul u_s18_tr (.a(r17_num), .b(r17_y2),   .result(c18_tanh_raw));
    fp32_mul u_s18_hx (.a(C_05),    .b(r17_x),    .result(c18_half_x));

    logic [31:0] r18_tanh_raw, r18_half_x;
    logic        r18_sat, r18_asign;
    always_ff @(posedge clk) begin
        if (rst) begin r18_tanh_raw <= '0; r18_half_x <= '0;
                       r18_sat <= '0; r18_asign <= '0; end
        else     begin r18_tanh_raw <= c18_tanh_raw; r18_half_x <= c18_half_x;
                       r18_sat <= r17_sat; r18_asign <= r17_asign; end
    end

    // =========================================================================
    // Stage 19: one_plus_tanh = 1.0 + tanh_val  [ADD; sat mux is free]
    // =========================================================================
    logic [31:0] c19_tanh_val, c19_opt;
    assign c19_tanh_val = r18_sat ? {r18_asign, 8'h7F, 23'h0} : r18_tanh_raw;
    bf16_fp32_add u_s19 (.a(C_1), .b(c19_tanh_val), .result(c19_opt));

    logic [31:0] r19_half_x, r19_opt;
    always_ff @(posedge clk) begin
        if (rst) begin r19_half_x <= '0; r19_opt <= '0; end
        else     begin r19_half_x <= r18_half_x; r19_opt <= c19_opt; end
    end

    // =========================================================================
    // Stage 20: result = half_x * one_plus_tanh
    // =========================================================================
    logic [31:0] c20_result;
    fp32_mul u_s20 (.a(r19_half_x), .b(r19_opt), .result(c20_result));

    always_ff @(posedge clk) begin
        if (rst) result <= '0;
        else     result <= c20_result;
    end

endmodule
