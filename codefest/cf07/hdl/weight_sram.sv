`timescale 1ns / 1ps

// Single-port weight SRAM — combinational read, registered write.
// Combinational read eliminates the 1-cycle latency that would require
// an extra PREFETCH state before LOAD_WT.

module weight_sram #(
    parameter COLS       = 32,
    parameter WT_WIDTH   = 16,
    parameter DEPTH      = 16,
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic                        clk,
    input  logic                        we,
    input  logic [ADDR_WIDTH-1:0]       addr,
    input  logic [COLS*WT_WIDTH-1:0]    wdata,
    output logic [COLS*WT_WIDTH-1:0]    rdata
);
    logic [COLS*WT_WIDTH-1:0] mem [0:DEPTH-1];

    always_ff @(posedge clk)
        if (we) mem[addr] <= wdata;

    assign rdata = mem[addr];   // combinational — no latency
endmodule
