`timescale 1ns / 1ps

// fp32_recip — FP32 reciprocal via bit-manipulation initial estimate
//              followed by two Newton-Raphson refinement iterations.
//
// All multiplications use bf16_mul (BF16 precision) which gives ≈0.78%
// relative error per operation.  The NR iteration is self-correcting:
//
//   Initial estimate: y₀ = 0x7EEEEEEE − x_bits   (≈ ±5% relative error)
//   Iteration:        yₙ₊₁ = yₙ × (2 − x × yₙ)  (error squares each pass)
//
//   After pass 1: ≈ 0.25%  (limited by BF16 rounding floor)
//   After pass 2: ≈ 0.78%  (BF16 rounding dominates — further passes unused)
//
// Inputs: x must be positive (denominator of Padé rational is always > 0).
// Special cases: x = 0 → +inf,  x = inf → 0.

module fp32_recip (
    input  logic [31:0] x,       // FP32, positive (sign bit = 0 for GELU den)
    output logic [31:0] result   // FP32 ≈ 1/x
);
    // ── Initial estimate (bit-manipulation trick) ─────────────────────────────
    // For FP32: x = 1.m × 2^{e-127}  →  1/x ≈ 1/1.m × 2^{127-e}
    // Subtracting the FP32 bit-pattern from 0x7EEEEEEE gives a ≈5% estimate.
    logic [31:0] y0;
    assign y0 = 32'h7EEEEEEE - {1'b0, x[30:0]};  // x is positive so sign=0

    // ── NR iteration 1: y1 = y0 × (2 − x × y0) ──────────────────────────────
    // (2 − x×y0) computed as: negate x×y0, then add 2.0
    logic [31:0] xy0, neg_xy0, err0, y1;

    bf16_mul u_m0 (.act_bf16(x [31:16]), .wt_bf16(y0 [31:16]), .product_fp32(xy0));
    assign   neg_xy0 = {~xy0[31], xy0[30:0]};                   // FP32 negation
    bf16_fp32_add u_a0 (.a(neg_xy0), .b(32'h40000000), .result(err0)); // 2 − xy0
    bf16_mul u_m1 (.act_bf16(y0 [31:16]), .wt_bf16(err0[31:16]), .product_fp32(y1));

    // ── NR iteration 2: y2 = y1 × (2 − x × y1) ──────────────────────────────
    logic [31:0] xy1, neg_xy1, err1, y2;

    bf16_mul u_m2 (.act_bf16(x [31:16]), .wt_bf16(y1 [31:16]), .product_fp32(xy1));
    assign   neg_xy1 = {~xy1[31], xy1[30:0]};
    bf16_fp32_add u_a1 (.a(neg_xy1), .b(32'h40000000), .result(err1)); // 2 − xy1
    bf16_mul u_m3 (.act_bf16(y1 [31:16]), .wt_bf16(err1[31:16]), .product_fp32(y2));

    // ── Special cases ─────────────────────────────────────────────────────────
    assign result = (x[30:23] == 8'h00) ? 32'h7F800000 :  // 1/0 = +inf
                    (x[30:23] == 8'hFF) ? 32'h00000000 :  // 1/inf = 0
                    y2;
endmodule
