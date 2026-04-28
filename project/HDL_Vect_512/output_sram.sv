`timescale 1ns / 1ps

// Dual-port output SRAM for the 128-wide vector MAC.
//
// Row 0 holds the 128-element psum output vector packed as 128 × 32 bits
// (4096 bits). Depth=2 avoids a 0-bit ADDR_WIDTH; only row 0 is used.
// The AXI readback serialises this into 8 × 512-bit AXI beats.
module output_sram #(
    parameter PSUM_WIDTH = 32,
    parameter VEC_SIZE   = 128,
    parameter DATA_WIDTH = PSUM_WIDTH * VEC_SIZE, // 4096
    parameter DEPTH      = 2,
    parameter ADDR_WIDTH = $clog2(DEPTH)          // 1
)(
    input  logic                    clk,
    // Write port – driven by controller at CAPTURE
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   waddr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    // Read port – driven by AXI readback
    input  logic                    re,
    input  logic [ADDR_WIDTH-1:0]   raddr,
    output logic [DATA_WIDTH-1:0]   rdata
);
    logic [DATA_WIDTH-1:0] mem [0:DEPTH-1];

    always_ff @(posedge clk) begin
        if (we) mem[waddr] <= wdata;
        if (re) rdata <= mem[raddr];
    end

endmodule
