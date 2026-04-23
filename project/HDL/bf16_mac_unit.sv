`timescale 1ns / 1ps

// Single-lane BF16 multiply-accumulate unit.
//
// Computes: acc_fp32_out = acc_fp32_in + (act_bf16 * wt_bf16)
//
// Data formats:
//   Inputs  — BF16 (16-bit): 1 sign | 8 exp | 7 mantissa (IEEE 754 truncated FP32)
//   Accumulator — FP32 (32-bit): 1 sign | 8 exp | 23 mantissa
//   Output  — FP32 (32-bit)
//
// BF16 → FP32 conversion is a free bit-extension: BF16 and FP32 share the same
// sign and exponent encoding, so {bf16[15:0], 16'b0} is a valid FP32.
//
// ─── Simulation model (this file) ────────────────────────────────────────────
// Uses SystemVerilog shortreal + $bitstoshortreal / $shortrealtobits to get
// true IEEE 754 FP32 results from the host FPU.  Verilator 5.x supports this.
//
// ─── Synthesis replacement (Berkeley HardFloat) ───────────────────────────────
// For tape-out, replace this module body with the HardFloat instantiation
// shown in the commented block at the bottom.  Fetch HardFloat:
//
//   git clone https://github.com/ucb-bar/berkeley-hardfloat.git
//   cd berkeley-hardfloat
//   sbt "runMain hardfloat.stage.HardFloatStage"   # generates Verilog in generated/
//
// Add the generated *.v files to your source list and use the synthesis body
// below.  The module port signature is identical — no other files change.
// ─────────────────────────────────────────────────────────────────────────────

module bf16_mac_unit (
    input  logic [15:0] act_bf16,      // activation scalar  (BF16)
    input  logic [15:0] wt_bf16,       // weight element     (BF16)
    input  logic [31:0] acc_fp32_in,   // accumulator input  (FP32)
    output logic [31:0] acc_fp32_out   // accumulator output (FP32): acc + act*wt
);

    // ── BF16 → FP32 (zero-extend the mantissa) ───────────────────────────
    logic [31:0] fp32_act, fp32_wt;
    assign fp32_act = {act_bf16, 16'b0};
    assign fp32_wt  = {wt_bf16,  16'b0};

    // ── IEEE 754 FP32 MAC via DPI-C (true float arithmetic) ──────────────
    // The shortreal approach is broken in simulation: the tool promotes
    // shortreal to double, so $shortrealtobits returns the wrong bits.
    // The C helper does genuine 32-bit float math via memcpy.
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

endmodule

// ════════════════════════════════════════════════════════════════════════════
// SYNTHESIS BODY — Berkeley HardFloat FP32 (expWidth=8, sigWidth=24)
// Replace the module body above with this block for ASIC/FPGA synthesis.
// Requires: recFNFromFN.v  mulRecFN.v  addRecFN.v  fNFromRecFN.v
// ════════════════════════════════════════════════════════════════════════════
//
// module bf16_mac_unit (
//     input  logic [15:0] act_bf16,
//     input  logic [15:0] wt_bf16,
//     input  logic [31:0] acc_fp32_in,
//     output logic [31:0] acc_fp32_out
// );
//     // BF16 → FP32 (free bit extension)
//     wire [31:0] fp32_act = {act_bf16, 16'b0};
//     wire [31:0] fp32_wt  = {wt_bf16,  16'b0};
//
//     // Convert IEEE FP32 → HardFloat recoded format (33 bits for expWidth=8)
//     wire [32:0] rec_act, rec_wt, rec_acc;
//     recFNFromFN #(.expWidth(8), .sigWidth(24)) u_conv_act (.in(fp32_act),      .out(rec_act));
//     recFNFromFN #(.expWidth(8), .sigWidth(24)) u_conv_wt  (.in(fp32_wt),       .out(rec_wt));
//     recFNFromFN #(.expWidth(8), .sigWidth(24)) u_conv_acc (.in(acc_fp32_in),   .out(rec_acc));
//
//     // FP32 multiply: act × wt
//     wire [32:0] rec_mul;
//     mulRecFN #(.expWidth(8), .sigWidth(24)) u_mul (
//         .control       (3'b0),       // default control
//         .a             (rec_act),
//         .b             (rec_wt),
//         .roundingMode  (3'b000),     // round-nearest-even
//         .out           (rec_mul),
//         .exceptionFlags(/* unused */)
//     );
//
//     // FP32 add: acc + product
//     wire [32:0] rec_result;
//     addRecFN #(.expWidth(8), .sigWidth(24)) u_add (
//         .control       (3'b0),
//         .a             (rec_mul),
//         .b             (rec_acc),
//         .subOp         (1'b0),
//         .roundingMode  (3'b000),
//         .out           (rec_result),
//         .exceptionFlags(/* unused */)
//     );
//
//     // Convert HardFloat recoded → IEEE FP32
//     fNFromRecFN #(.expWidth(8), .sigWidth(24)) u_deconv (.in(rec_result), .out(acc_fp32_out));
// endmodule
