`timescale 1ns / 1ps

// AXI4-Stream readback for the streaming systolic accelerator.
//
// Reads M_count rows from accum_sram and streams them as AXI beats.
// Each row is COLS×PSUM_WIDTH = 32×32 = 1024 bits = 2×512-bit beats.
// Total beats = 2 × M_count (e.g. 512 beats for M=256).
//
// sram_addr is driven combinationally from the beat counter so the
// accum_sram's combinational read is valid in the same cycle.
// tvalid/tlast/tdata are all combinational.

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

    input  logic [M_ADDR_W-1:0]        M_count,
    output logic [M_ADDR_W-1:0]        sram_addr,
    input  logic [COLS*PSUM_WIDTH-1:0]  sram_rdata,

    output logic [AXI_WIDTH-1:0] m_axis_tdata,
    output logic                 m_axis_tvalid,
    input  logic                 m_axis_tready,
    output logic                 m_axis_tlast
);
    // 2 beats per row (1024 bits / 512 bits)
    localparam VALS_PER_BEAT = AXI_WIDTH / PSUM_WIDTH;   // 16

    // beat_cnt counts total beats: 0 .. 2*M_count-1
    // Uses one extra bit beyond M_ADDR_W to accommodate 2×M_count
    logic [M_ADDR_W:0] beat_cnt;
    logic              running;

    // Address into accum_sram = beat / 2
    assign sram_addr = beat_cnt[M_ADDR_W:1];

    // Which half of the 1024-bit row: bit 0 of beat_cnt
    logic beat_sel;
    assign beat_sel = beat_cnt[0];

    // Pack the correct 512-bit slice from the 1024-bit SRAM row
    genvar i;
    generate
        for (i = 0; i < VALS_PER_BEAT; i++) begin : g_tdata
            assign m_axis_tdata[i*PSUM_WIDTH +: PSUM_WIDTH] =
                beat_sel
                ? sram_rdata[(i + VALS_PER_BEAT)*PSUM_WIDTH +: PSUM_WIDTH]
                : sram_rdata[i*PSUM_WIDTH +: PSUM_WIDTH];
        end
    endgenerate

    // Total beats = 2 * M_count.  Last beat index = 2*M_count - 1.
    logic [M_ADDR_W:0] total_beats;
    assign total_beats = {M_count, 1'b0};  // M_count << 1

    assign m_axis_tvalid = running;
    assign m_axis_tlast  = running && (beat_cnt == total_beats - 1'b1);
    assign busy          = running;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            running  <= 1'b0;
            beat_cnt <= '0;
        end else if (!running && start) begin
            running  <= 1'b1;
            beat_cnt <= '0;
        end else if (running && m_axis_tready) begin
            if (beat_cnt == total_beats - 1'b1)
                running <= 1'b0;
            else
                beat_cnt <= beat_cnt + 1'b1;
        end
    end
endmodule
