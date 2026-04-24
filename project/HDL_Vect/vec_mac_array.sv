`timescale 1ns / 1ps

// 128-wide BF16 vector MAC array.
//
// Each cycle, every accumulator computes:
//   acc[j] (FP32) += act_in (BF16) * wt_in[j] (BF16)
// where act_in is a scalar broadcast to all 128 lanes and wt_in is a
// 2048-bit packed word holding 128 × 16-bit BF16 weight elements.
//
// The per-lane multiply-accumulate is performed by bf16_mac_unit, which
// converts BF16 inputs to FP32 (trivial bit extension) and computes the
// FP32 result.  See bf16_mac_unit.sv for the simulation/synthesis split.
//
// Accumulators hold FP32 (32-bit).  FP32 0.0 = 32'h0000_0000, so the
// reset value of '0 and the first-tile seed of '0 are both correct.
//
// load and mac_en are mutually exclusive; the controller guarantees this.
module vec_mac_array #(
    parameter VEC_SIZE   = 128,
    parameter ACT_WIDTH  = 16,   // BF16
    parameter WT_WIDTH   = 16,   // BF16
    parameter PSUM_WIDTH = 32    // FP32 accumulator
)(
    input  logic clk,
    input  logic rst_n,

    input  logic load,   // seed accumulators from psum_seed (start of tile)
    input  logic mac_en, // BF16 multiply-accumulate enable

    input  logic [ACT_WIDTH-1:0]              act_in,    // BF16 scalar broadcast
    input  logic [VEC_SIZE*WT_WIDTH-1:0]      wt_in,     // packed 128 × BF16
    input  logic [VEC_SIZE*PSUM_WIDTH-1:0]    psum_seed, // FP32 inter-tile init
    output logic [VEC_SIZE*PSUM_WIDTH-1:0]    psum_out   // FP32 packed 128 × 32-bit
);

    genvar j;
    generate
        for (j = 0; j < VEC_SIZE; j++) begin : g_acc
            logic [PSUM_WIDTH-1:0] acc;        // FP32 accumulator register
            logic [PSUM_WIDTH-1:0] mac_result; // combinational output of bf16_mac_unit

            assign psum_out[j*PSUM_WIDTH +: PSUM_WIDTH] = acc;

            // Combinational BF16 MAC: mac_result = acc + act_in * wt_in[j]
            // keep_hierarchy instructs Genus to synthesize each instance
            // independently rather than flattening all 128 — critical for
            // runtime and memory on repetitive datapath structures.
            (* keep_hierarchy = "yes" *)
            bf16_mac_unit u_mac (
                .act_bf16    (act_in),
                .wt_bf16     (wt_in[j*WT_WIDTH +: WT_WIDTH]),
                .acc_fp32_in (acc),
                .acc_fp32_out(mac_result)
            );

            always_ff @(posedge clk or negedge rst_n) begin
                if (!rst_n)
                    acc <= 32'h0000_0000;   // FP32 +0.0
                else if (load)
                    acc <= psum_seed[j*PSUM_WIDTH +: PSUM_WIDTH];
                else if (mac_en)
                    acc <= mac_result;
            end
        end
    endgenerate

endmodule
