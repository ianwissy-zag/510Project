`timescale 1ns / 1ps

// M×COLS FP32 accumulator SRAM — two combinational read ports, one write port.
//
// rd_addr_a: driven by controller for psum_in[0] feed (m_in-1)
// rd_addr_b: driven by axi_readback for streaming results out
// wr_addr:   driven by controller for output capture (m_out)
//
// During STREAM: rd_addr_a = m_in-1, wr_addr = m_out = m_in - ROWS.
// These are always ROWS-1 apart → never alias → no read-write hazard.
// During readback: rd_addr_b iterates 0..M_count-1; controller is idle.

module accum_sram #(
    parameter COLS       = 32,
    parameter PSUM_WIDTH = 32,
    parameter M_MAX      = 256,
    parameter ADDR_WIDTH = $clog2(M_MAX + 1)
)(
    input  logic clk,

    // Write port
    input  logic                       we,
    input  logic [ADDR_WIDTH-1:0]      wr_addr,
    input  logic [COLS*PSUM_WIDTH-1:0] wdata,

    // Read port A — psum_in[0] feed
    input  logic [ADDR_WIDTH-1:0]      rd_addr_a,
    output logic [COLS*PSUM_WIDTH-1:0] rdata_a,

    // Read port B — AXI readback
    input  logic [ADDR_WIDTH-1:0]      rd_addr_b,
    output logic [COLS*PSUM_WIDTH-1:0] rdata_b
);
    logic [COLS*PSUM_WIDTH-1:0] mem [0:M_MAX-1];

    always_ff @(posedge clk)
        if (we) mem[wr_addr] <= wdata;

    assign rdata_a = mem[rd_addr_a];
    assign rdata_b = mem[rd_addr_b];
endmodule
