`timescale 1ns / 1ps

// Dual-port activation SRAM for the 128-wide vector MAC.
//
// Each row holds the entire K-element activation vector packed as
// K × ACT_WIDTH bits (512 bits for K=32, ACT_WIDTH=16).
// Depth=2 provides the ping/pong pair; the host writes one buffer while
// the controller reads the other.
module act_sram #(
    parameter DATA_WIDTH = 512,          // K_DEPTH * ACT_WIDTH = 32 * 16
    parameter DEPTH      = 2,            // ping (row 0) + pong (row 1)
    parameter ADDR_WIDTH = $clog2(DEPTH) // 1
)(
    input  logic                    clk,
    // Write port – driven by AXI stream
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   waddr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    // Read port – driven by controller
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
