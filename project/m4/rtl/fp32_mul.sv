`timescale 1ns / 1ps

// FP32×FP32 combinational multiplier — result in FP32 format.
//
// Computes: result = a * b  (both FP32)
//
// FP32: [31] sign | [30:23] exponent (bias=127) | [22:0] mantissa
// Mantissa with implicit leading 1: 24 bits; 24×24 = 48-bit product.
//
// Special case handling:
//   Zero/denormal inputs  → zero output (flush-to-zero for denormals)
//   Infinity inputs       → infinity output
//   Exponent overflow     → infinity
//   Exponent underflow    → zero
//
// Rounding: truncation (round-toward-zero).

module fp32_mul (
    input  logic [31:0] a,
    input  logic [31:0] b,
    output logic [31:0] result
);
    logic       s_a, s_b;
    logic [7:0] e_a, e_b;
    logic [23:0] m_a, m_b;

    assign s_a = a[31]; assign e_a = a[30:23]; assign m_a = {1'b1, a[22:0]};
    assign s_b = b[31]; assign e_b = b[30:23]; assign m_b = {1'b1, b[22:0]};

    logic zero_a, zero_b, inf_a, inf_b, denorm_a, denorm_b;
    assign zero_a   = (e_a == 8'h00) && (a[22:0] == 23'h0);
    assign zero_b   = (e_b == 8'h00) && (b[22:0] == 23'h0);
    assign inf_a    = (e_a == 8'hFF);
    assign inf_b    = (e_b == 8'hFF);
    assign denorm_a = (e_a == 8'h00);
    assign denorm_b = (e_b == 8'h00);

    logic s_r;
    assign s_r = s_a ^ s_b;

    // 24×24 → 48-bit mantissa product
    logic [47:0] mant_prod;
    assign mant_prod = m_a * m_b;

    // bit47=1 → product ≥ 2.0; shift right 1, exp++
    logic        norm_shift;
    logic [22:0] norm_mant;
    assign norm_shift = mant_prod[47];
    assign norm_mant  = norm_shift ? mant_prod[46:24] : mant_prod[45:23];

    logic signed [9:0] exp_result;
    assign exp_result = $signed({2'b0, e_a}) + $signed({2'b0, e_b})
                        - 10'sd127 + $signed({9'b0, norm_shift});

    always_comb begin
        if (zero_a || zero_b || denorm_a || denorm_b)
            result = {s_r, 31'b0};
        else if (inf_a || inf_b || (exp_result >= 10'sd255))
            result = {s_r, 8'hFF, 23'b0};
        else if (exp_result <= 10'sd0)
            result = {s_r, 31'b0};
        else
            result = {s_r, exp_result[7:0], norm_mant};
    end
endmodule
