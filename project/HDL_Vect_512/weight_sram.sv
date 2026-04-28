`timescale 1ns / 1ps

// Single-port synchronous weight SRAM for the 128-wide vector MAC.
//
// Each row holds one full set of 128 × 16-bit weights (2048 bits).
// The AXI input buffer assembles 4 × 512-bit AXI beats into the full
// 2048-bit row before writing. Write and compute phases are non-overlapping.
module weight_sram #(
    parameter DATA_WIDTH = 2048,         // VEC_SIZE * WT_WIDTH = 128 * 16
    parameter DEPTH      = 32,           // K_DEPTH accumulation steps
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic                    clk,
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   addr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    output logic [DATA_WIDTH-1:0]   rdata
);
    logic [DATA_WIDTH-1:0] mem [0:DEPTH-1];

    always_ff @(posedge clk) begin
        if (we) mem[addr] <= wdata;
        rdata <= mem[addr]; // read-before-write
    end

endmodule
