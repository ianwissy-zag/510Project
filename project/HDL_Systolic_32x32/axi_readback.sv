`timescale 1ns / 1ps

// AXI4-Stream readback with synthesisable bias addition and GELU.
//
// Post-processing pipeline per output column (combinational):
//   accum[m][c]  →  bf16_fp32_add(+bias[c])  →  gelu_unit  →  AXI beat
//
// apply_bias : add bias[c] to each column before GELU/output
// apply_gelu : apply GELU(Padé[3/2]) after bias
// Both are latched at rb_start and held for the burst.
//
// GELU uses gelu_unit.sv (bf16_mul + fp32_recip, fully synthesisable).
// Bias uses bf16_fp32_add.sv (FP32+FP32 adder, already in the design).
//
// Beat structure: 2 beats per M row, 512 bits (16 FP32 values) each.

module axi_readback #(
    parameter COLS       = 32,
    parameter PSUM_WIDTH = 32,
    parameter AXI_WIDTH  = 512,
    parameter M_MAX      = 256,
    parameter M_ADDR_W   = $clog2(M_MAX + 1)
)(
    input  logic clk,
    input  logic rst_n,
    input  logic start,
    output logic busy,

    // Post-processing controls (sampled at start)
    input  logic apply_bias,
    input  logic apply_gelu,

    // accum_sram read port
    input  logic [M_ADDR_W-1:0]        M_count,
    output logic [M_ADDR_W-1:0]        sram_addr,
    input  logic [COLS*PSUM_WIDTH-1:0] sram_rdata,

    // bias_sram read port — all COLS values in parallel
    input  logic [COLS-1:0][PSUM_WIDTH-1:0] bias_rdata,

    // AXI-S master
    output logic [AXI_WIDTH-1:0] m_axis_tdata,
    output logic                 m_axis_tvalid,
    input  logic                 m_axis_tready,
    output logic                 m_axis_tlast
);
    localparam VALS_PER_BEAT = AXI_WIDTH / PSUM_WIDTH;  // 16

    logic [M_ADDR_W:0] beat_cnt;
    logic              running;
    logic              rb_apply_bias, rb_apply_gelu;

    assign sram_addr = beat_cnt[M_ADDR_W:1];
    logic beat_sel;
    assign beat_sel = beat_cnt[0];

    // ── Per-column post-processing ────────────────────────────────────────────
    // For each of the COLS=32 output columns:
    //   raw      = accum_sram value for this column
    //   biased   = raw + bias[c]          (always computed, muxed on apply_bias)
    //   gelu_in  = biased or raw          (selected by apply_bias flag)
    //   gelu_out = GELU(gelu_in)          (always computed, muxed on apply_gelu)
    //   processed = gelu_out or gelu_in   (selected by apply_gelu flag)
    //
    // All arithmetic modules are combinational — synthesis adds registers
    // for timing closure as needed.

    logic [COLS-1:0][PSUM_WIDTH-1:0] processed;

    genvar c;
    generate
        for (c = 0; c < COLS; c++) begin : g_postproc
            logic [PSUM_WIDTH-1:0] raw, biased_val, gelu_in, gelu_out;

            assign raw = sram_rdata[c*PSUM_WIDTH +: PSUM_WIDTH];

            // Bias: FP32 addition using existing bf16_fp32_add
            bf16_fp32_add u_bias (
                .a(raw),
                .b(bias_rdata[c]),
                .result(biased_val)
            );

            // Select GELU input based on apply_bias
            assign gelu_in = rb_apply_bias ? biased_val : raw;

            // GELU: Padé [3/2] approximation, fully synthesisable
            gelu_unit u_gelu (
                .x(gelu_in),
                .result(gelu_out)
            );

            // Output select
            assign processed[c] = rb_apply_gelu ? gelu_out : gelu_in;
        end
    endgenerate

    // ── AXI beat packing ──────────────────────────────────────────────────────
    genvar i;
    generate
        for (i = 0; i < VALS_PER_BEAT; i++) begin : g_tdata
            assign m_axis_tdata[i*PSUM_WIDTH +: PSUM_WIDTH] =
                beat_sel ? processed[i + VALS_PER_BEAT] : processed[i];
        end
    endgenerate

    logic [M_ADDR_W:0] total_beats;
    assign total_beats = {M_count, 1'b0};

    assign m_axis_tvalid = running;
    assign m_axis_tlast  = running && (beat_cnt == total_beats - 1'b1);
    assign busy          = running;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            running       <= 1'b0;
            beat_cnt      <= '0;
            rb_apply_bias <= 1'b0;
            rb_apply_gelu <= 1'b0;
        end else if (!running && start) begin
            running       <= 1'b1;
            beat_cnt      <= '0;
            rb_apply_bias <= apply_bias;
            rb_apply_gelu <= apply_gelu;
        end else if (running && m_axis_tready) begin
            if (beat_cnt == total_beats - 1'b1)
                running <= 1'b0;
            else
                beat_cnt <= beat_cnt + 1'b1;
        end
    end
endmodule
