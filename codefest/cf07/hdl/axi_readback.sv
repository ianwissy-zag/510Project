`timescale 1ns / 1ps

// AXI4-Stream readback with pipelined bias addition and GELU.
//
// The gelu_unit has a 5-cycle pipeline latency.  A warmup counter runs for
// GELU_LATENCY=5 cycles before the AXI output begins, feeding the SRAM
// address sequence from 0 so that by the time the first beat is transmitted
// the pipeline output is correctly aligned with M row 0.
//
// SRAM address sequence across the full readback (warmup + output):
//   total_cnt 0,1  → addr=0  (warm-up, pipeline filling, tvalid=0)
//   total_cnt 2,3  → addr=1  (warm-up)
//   total_cnt 4    → addr=2  (warm-up, last cycle)
//   total_cnt 5,6  → addr=0  (output beat 0,1  — gelu output = M row 0) ✓
//   total_cnt 7,8  → addr=1  (output beat 2,3  — gelu output = M row 1) ✓
//   ...
//   total_cnt 5+2*(M-1), 5+2*(M-1)+1 → addr=M+1 (gelu output = M row M-1) ✓
//
// Because the SRAM is combinational and all data was written before readback
// starts, re-reading an address that was already presented during warm-up is
// harmless — the result is identical.
//
// Total cycles per readback: GELU_LATENCY + 2 × M_count
// HAL sees exactly 2 × M_count valid AXI beats (unchanged interface).

module axi_readback #(
    parameter COLS        = 32,
    parameter PSUM_WIDTH  = 32,
    parameter AXI_WIDTH   = 512,
    parameter M_MAX       = 256,
    parameter M_ADDR_W    = $clog2(M_MAX + 1),
    parameter GELU_LATENCY = 9    // 8 gelu_unit banks + 1 input pre-register
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

    // bias_sram read port
    input  logic [COLS-1:0][PSUM_WIDTH-1:0] bias_rdata,

    // AXI-S master
    output logic [AXI_WIDTH-1:0] m_axis_tdata,
    output logic                 m_axis_tvalid,
    input  logic                 m_axis_tready,
    output logic                 m_axis_tlast
);
    localparam VALS_PER_BEAT = AXI_WIDTH / PSUM_WIDTH;  // 16

    // total_cnt spans warmup + output: 0 .. GELU_LATENCY + 2*M_count - 1
    // Use a wide enough counter: M_ADDR_W+1 bits for 2*M_count, plus log2(LATENCY)
    localparam CNT_W = M_ADDR_W + 4;  // sufficient for 5 + 2*256 = 517

    logic [CNT_W-1:0] total_cnt;
    logic             running;
    logic             rb_apply_bias, rb_apply_gelu;

    // Effective latency: GELU_LATENCY when GELU is active, 0 otherwise.
    // This keeps the non-GELU path latency-free while still pre-feeding the
    // pipeline when GELU is enabled.
    logic [CNT_W-1:0] lat;
    assign lat = rb_apply_gelu ? CNT_W'(GELU_LATENCY) : '0;

    // SRAM address: driven by total_cnt so pipeline is filled LATENCY cycles
    // before the first valid AXI beat.
    assign sram_addr = total_cnt[M_ADDR_W:1];

    // Output-relative counter: 0 on the first AXI beat regardless of warmup.
    // Using this for beat_sel keeps lower/upper half parity always correct.
    logic [CNT_W-1:0] out_cnt;
    assign out_cnt  = total_cnt - lat;

    logic beat_sel;
    assign beat_sel = out_cnt[0];

    // ── Per-column post-processing ────────────────────────────────────────────
    logic [COLS-1:0][PSUM_WIDTH-1:0] processed;

    genvar c;
    generate
        for (c = 0; c < COLS; c++) begin : g_postproc
            logic [PSUM_WIDTH-1:0] raw, biased_val, gelu_in, gelu_out;

            assign raw     = sram_rdata[c*PSUM_WIDTH +: PSUM_WIDTH];

            bf16_fp32_add u_bias (
                .a(raw), .b(bias_rdata[c]), .result(biased_val));

            assign gelu_in = rb_apply_bias ? biased_val : raw;

            // Register gelu_in to break the SRAM+bias+x2+x3 critical path.
            // Without this register the path is ~1800 ps (violates 1650 ps).
            // With it: SRAM+bias → reg (~1200 ps) and reg→x2→x3→R0 (~600 ps).
            logic [PSUM_WIDTH-1:0] gelu_in_r;
            always_ff @(posedge clk or negedge rst_n) begin
                if (!rst_n) gelu_in_r <= '0;
                else        gelu_in_r <= gelu_in;
            end

            gelu_unit u_gelu (
                .clk(clk), .rst(~rst_n),
                .x(gelu_in_r), .result(gelu_out));

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

    // ── Timing control ────────────────────────────────────────────────────────
    logic [CNT_W-1:0] total_beats;
    assign total_beats = lat + {M_count, 1'b0};  // warmup + 2*M_count

    logic in_warmup;
    assign in_warmup = (total_cnt < lat);

    assign m_axis_tvalid = running && !in_warmup;
    assign m_axis_tlast  = running && !in_warmup &&
                           (total_cnt == total_beats - 1'b1);
    assign busy          = running;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            running       <= 1'b0;
            total_cnt     <= '0;
            rb_apply_bias <= 1'b0;
            rb_apply_gelu <= 1'b0;
        end else if (!running && start) begin
            running       <= 1'b1;
            total_cnt     <= '0;
            rb_apply_bias <= apply_bias;
            rb_apply_gelu <= apply_gelu;
        end else if (running) begin
            // Advance on every cycle during warmup; on tready during output.
            if (in_warmup || m_axis_tready) begin
                if (total_cnt == total_beats - 1'b1)
                    running <= 1'b0;
                else
                    total_cnt <= total_cnt + 1'b1;
            end
        end
    end
endmodule
