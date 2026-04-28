`timescale 1ns / 1ps

// Custom BF16×BF16 multiplier — result in FP32 format.
//
// Computes: product_fp32 = act_bf16 * wt_bf16
//
// BF16: [15] sign | [14:7] exponent (bias=127) | [6:0] mantissa
// FP32: [31] sign | [30:23] exponent (bias=127) | [22:0] mantissa
//
// BF16 has 8 significant mantissa bits (7 explicit + 1 implicit leading).
// The product of two 8-bit mantissas is at most 16 bits, giving 14 explicit
// mantissa bits after normalization. These fit exactly in FP32's 23-bit
// mantissa field with no rounding required — the bottom 9 bits are zero.
//
// Special case handling:
//   Zero inputs     → zero output (sign preserved)
//   Denormal inputs → flushed to zero (standard for ML hardware)
//   Infinity inputs → infinity output
//   Exponent overflow  → infinity
//   Exponent underflow → zero
//
// NaN propagation omitted — not needed for well-conditioned ML training.

module bf16_mul (
    input  logic [15:0] act_bf16,
    input  logic [15:0] wt_bf16,
    output logic [31:0] product_fp32
);

    // ── Field extraction ──────────────────────────────────────────────────
    logic       s_a, s_b;
    logic [7:0] e_a, e_b;
    logic [7:0] m_a, m_b;     // 8-bit with implicit leading 1 prepended

    assign s_a = act_bf16[15];
    assign e_a = act_bf16[14:7];
    assign m_a = {1'b1, act_bf16[6:0]};

    assign s_b = wt_bf16[15];
    assign e_b = wt_bf16[14:7];
    assign m_b = {1'b1, wt_bf16[6:0]};

    // ── Special case detection ────────────────────────────────────────────
    logic zero_a, zero_b, inf_a, inf_b, denorm_a, denorm_b;

    assign zero_a   = (e_a == 8'h00) && (act_bf16[6:0] == 7'h00);
    assign zero_b   = (e_b == 8'h00) && (wt_bf16[6:0]  == 7'h00);
    assign inf_a    = (e_a == 8'hFF);
    assign inf_b    = (e_b == 8'hFF);
    assign denorm_a = (e_a == 8'h00);
    assign denorm_b = (e_b == 8'h00);

    // ── Result sign ───────────────────────────────────────────────────────
    logic s_r;
    assign s_r = s_a ^ s_b;

    // ── Mantissa multiply: 8×8 → 16 bits ─────────────────────────────────
    logic [15:0] mant_prod;
    assign mant_prod = m_a * m_b;

    // ── Normalization ─────────────────────────────────────────────────────
    // Normal×normal: product is in [1.0000000_00000000, 11.1111110_00000001]
    // mant_prod[15]=1 → 1x.xxxxxxxxxxxxxx, shift right 1, exponent += 1
    // mant_prod[15]=0 → 01.xxxxxxxxxxxxx, bit 14 is implicit leading 1
    //
    // In both cases the FP32 mantissa (23 bits, no implicit 1) is 14 bits
    // of significant data padded with 9 zeros.
    logic        norm_shift;
    logic [22:0] norm_mant;

    assign norm_shift = mant_prod[15];
    assign norm_mant  = norm_shift ?
        {mant_prod[14:0], 8'b0} :   // bit15=1: mantissa is bits[46:24] of 48-bit product
        {mant_prod[13:0], 9'b0};    // bit15=0: mantissa is bits[45:23] of 48-bit product

    // ── Exponent calculation ───────────────────────────────────────────────
    // Unbiased product exp = (e_a − 127) + (e_b − 127) = e_a + e_b − 254
    // Rebiased for FP32:   = e_a + e_b − 254 + 127 = e_a + e_b − 127
    // Plus norm_shift if mantissa product overflowed into bit 15.
    logic signed [9:0] exp_result;
    logic              exp_overflow, exp_underflow;

    assign exp_result    = $signed({2'b0, e_a}) + $signed({2'b0, e_b})
                           - 10'sd127 + $signed({9'b0, norm_shift});
    assign exp_overflow  = (exp_result >= 10'sd255);
    assign exp_underflow = (exp_result <= 10'sd0);

    // ── Output mux ────────────────────────────────────────────────────────
    always_comb begin
        if (zero_a || zero_b || denorm_a || denorm_b) begin
            product_fp32 = {s_r, 31'b0};            // zero
        end else if (inf_a || inf_b || exp_overflow) begin
            product_fp32 = {s_r, 8'hFF, 23'b0};    // infinity
        end else if (exp_underflow) begin
            product_fp32 = {s_r, 31'b0};            // underflow → zero
        end else begin
            product_fp32 = {s_r, exp_result[7:0], norm_mant};
        end
    end

endmodule
