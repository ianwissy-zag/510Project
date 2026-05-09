`timescale 1ns / 1ps

// gelu_unit — Synthesisable GELU(x) using Padé [3/2] tanh approximation.
//
// GELU(x) = 0.5 · x · (1 + tanh(√(2/π) · (x + 0.044715·x³)))
//
// tanh approximated via Padé [3/2] in x²:
//   tanh(a) ≈ a·(1485 + 171·a² + 2·a⁴) / (1485 + 666·a² + 26·a⁴)
//   Saturated to ±1 for |a| ≥ 4  (tanh(4) = 0.9993, < 0.07% saturation error)
//
// Division computed via fp32_recip (bit-trick + 2 Newton-Raphson iterations).
// All multiplications use bf16_mul (BF16 precision reusing existing hardware).
// All additions use bf16_fp32_add (FP32 precision).
//
// Fully combinational — synthesis pipelines for timing closure.
//
// BF16 constants (16-bit, upper 16 bits of nearest FP32):
//   0.044715 → 16'h3D37  (≈ 0.044678, −0.08% error)
//   √(2/π)   → 16'h3F4C  (≈ 0.796875, −0.13% error)
//   171.0    → 16'h432B  (exact in BF16)
//   2.0      → 16'h4000  (exact)
//   666.0    → 16'h4426  (≈ 664.0,    −0.30% error)
//   26.0     → 16'h41D0  (exact)
//   0.5      → 16'h3F00  (exact)
//
// FP32 constants (32-bit, for bf16_fp32_add):
//   1485.0   → 32'h44B9A000  (exact in FP32)
//   1.0      → 32'h3F800000  (exact)

module gelu_unit (
    input  logic [31:0] x,      // FP32 input (bias-adjusted accumulator value)
    output logic [31:0] result  // FP32 GELU(x)
);
    // ── BF16 input ────────────────────────────────────────────────────────────
    logic [15:0] x_bf16;
    assign x_bf16 = x[31:16];

    // ── Step 1: x² = x × x ───────────────────────────────────────────────────
    logic [31:0] x2;
    bf16_mul u_x2 (.act_bf16(x_bf16), .wt_bf16(x_bf16), .product_fp32(x2));

    // ── Step 2: x³ = x × x² ──────────────────────────────────────────────────
    logic [31:0] x3;
    bf16_mul u_x3 (.act_bf16(x_bf16), .wt_bf16(x2[31:16]), .product_fp32(x3));

    // ── Step 3: 0.044715 × x³ ────────────────────────────────────────────────
    logic [31:0] cube_term;
    bf16_mul u_ct (.act_bf16(16'h3D37), .wt_bf16(x3[31:16]), .product_fp32(cube_term));

    // ── Step 4: x + 0.044715·x³ ──────────────────────────────────────────────
    logic [31:0] inner;
    bf16_fp32_add u_in (.a(cube_term), .b(x), .result(inner));

    // ── Step 5: arg = √(2/π) × inner ─────────────────────────────────────────
    logic [31:0] arg;
    bf16_mul u_arg (.act_bf16(16'h3F4C), .wt_bf16(inner[31:16]), .product_fp32(arg));

    // ── Step 6: saturation flag — |arg| ≥ 4.0 ────────────────────────────────
    logic sat;
    assign sat = ({1'b0, arg[30:0]} >= 32'h40800000);

    // ── Step 7: arg² = arg × arg,   arg⁴ = arg² × arg² ─────────────────────
    logic [31:0] arg2, arg4;
    bf16_mul u_a2 (.act_bf16(arg[31:16]),  .wt_bf16(arg[31:16]),  .product_fp32(arg2));
    bf16_mul u_a4 (.act_bf16(arg2[31:16]), .wt_bf16(arg2[31:16]), .product_fp32(arg4));

    // ── Step 8: Padé polynomial terms ────────────────────────────────────────
    logic [31:0] a171, a2_x, a666, a26;
    bf16_mul u_171 (.act_bf16(16'h432B), .wt_bf16(arg2[31:16]), .product_fp32(a171)); // 171·a²
    bf16_mul u_2x  (.act_bf16(16'h4000), .wt_bf16(arg4[31:16]), .product_fp32(a2_x)); // 2·a⁴
    bf16_mul u_666 (.act_bf16(16'h4426), .wt_bf16(arg2[31:16]), .product_fp32(a666)); // 666·a²
    bf16_mul u_26  (.act_bf16(16'h41D0), .wt_bf16(arg4[31:16]), .product_fp32(a26));  // 26·a⁴

    // ── Step 9: num_inner = 1485 + 171·a² + 2·a⁴ ────────────────────────────
    logic [31:0] t_num, num_inner;
    bf16_fp32_add u_t1 (.a(32'h44B9A000), .b(a171), .result(t_num));
    bf16_fp32_add u_ni (.a(t_num),         .b(a2_x), .result(num_inner));

    // ── Step 10: den = 1485 + 666·a² + 26·a⁴ ────────────────────────────────
    logic [31:0] t_den, den;
    bf16_fp32_add u_t2 (.a(32'h44B9A000), .b(a666), .result(t_den));
    bf16_fp32_add u_dn (.a(t_den),         .b(a26),  .result(den));

    // ── Step 11: num = arg × num_inner ───────────────────────────────────────
    logic [31:0] num;
    bf16_mul u_nm (.act_bf16(arg[31:16]), .wt_bf16(num_inner[31:16]), .product_fp32(num));

    // ── Step 12: 1/den via Newton-Raphson ────────────────────────────────────
    logic [31:0] inv_den;
    fp32_recip u_rc (.x(den), .result(inv_den));

    // ── Step 13: tanh_raw = num × (1/den) ────────────────────────────────────
    logic [31:0] tanh_raw;
    bf16_mul u_tr (.act_bf16(num[31:16]), .wt_bf16(inv_den[31:16]), .product_fp32(tanh_raw));

    // ── Step 14: apply saturation ─────────────────────────────────────────────
    // For |arg| ≥ 4: tanh = ±1.0  (sign matches arg)
    logic [31:0] tanh_val;
    assign tanh_val = sat ? {arg[31], 8'h7F, 23'h0} : tanh_raw;

    // ── Step 15: 1 + tanh ─────────────────────────────────────────────────────
    logic [31:0] one_plus_tanh;
    bf16_fp32_add u_1t (.a(32'h3F800000), .b(tanh_val), .result(one_plus_tanh));

    // ── Step 16: 0.5 × x × (1 + tanh) ───────────────────────────────────────
    logic [31:0] half_x;
    bf16_mul u_hx (.act_bf16(16'h3F00), .wt_bf16(x_bf16),            .product_fp32(half_x));
    bf16_mul u_rs (.act_bf16(half_x[31:16]), .wt_bf16(one_plus_tanh[31:16]), .product_fp32(result));

endmodule
