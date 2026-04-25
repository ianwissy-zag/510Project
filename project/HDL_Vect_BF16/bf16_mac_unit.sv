`timescale 1ns / 1ps

// Single-lane BF16 multiply-accumulate unit.
//
// Computes: acc_fp32_out = acc_fp32_in + (act_bf16 * wt_bf16)
//
// Data formats:
//   Inputs      — BF16 (16-bit): 1 sign | 8 exp | 7 mantissa (truncated FP32)
//   Accumulator — FP32 (32-bit): 1 sign | 8 exp | 23 mantissa
//   Output      — FP32 (32-bit)
//
// BF16 → FP32 conversion is a free bit-extension: BF16 and FP32 share the same
// sign and exponent encoding, so {bf16[15:0], 16'b0} is a valid FP32.
//
// ── Simulation (`ifndef SYNTHESIS) ───────────────────────────────────────────
// DPI-C helper (bf16_mac_dpi.c) performs true 32-bit float arithmetic via
// memcpy on the host FPU. Required because Verilator promotes shortreal to
// double, making $shortrealtobits return incorrect bit patterns.
//
// ── Synthesis (`ifdef SYNTHESIS) ─────────────────────────────────────────────
// Custom BF16×BF16 multiplier (bf16_mul) + FP32 adder (bf16_fp32_add).
// Smaller and faster than the full HardFloat core by exploiting the limited
// 8-bit mantissa precision of BF16 inputs.

module bf16_mac_unit (
    input  logic [15:0] act_bf16,      // activation scalar  (BF16)
    input  logic [15:0] wt_bf16,       // weight element     (BF16)
    input  logic [31:0] acc_fp32_in,   // accumulator input  (FP32)
    output logic [31:0] acc_fp32_out   // accumulator output (FP32): acc + act*wt
);

`ifdef SYNTHESIS

    // ── Custom BF16×BF16 multiply + FP32 accumulate ───────────────────────────
    logic [31:0] product_fp32;

    bf16_mul u_mul (
        .act_bf16     (act_bf16),
        .wt_bf16      (wt_bf16),
        .product_fp32 (product_fp32)
    );

    bf16_fp32_add u_add (
        .a      (product_fp32),
        .b      (acc_fp32_in),
        .result (acc_fp32_out)
    );

`else

    // ── BF16 → FP32 (zero-extend the mantissa) ───────────────────────────────
    logic [31:0] fp32_act, fp32_wt;
    assign fp32_act = {act_bf16, 16'b0};
    assign fp32_wt  = {wt_bf16,  16'b0};

    // ── IEEE 754 FP32 MAC via DPI-C (true float arithmetic) ──────────────────
    // The tool promotes shortreal to double, so $shortrealtobits returns the
    // wrong bits. The C helper does genuine 32-bit float math via memcpy.
    import "DPI-C" function int bf16_mac_fp32(
        input int act_fp32_bits,
        input int wt_fp32_bits,
        input int acc_fp32_bits
    );

    always_comb begin
        acc_fp32_out = 32'(unsigned'(bf16_mac_fp32(
            int'(fp32_act),
            int'(fp32_wt),
            int'(acc_fp32_in)
        )));
    end

`endif

endmodule
