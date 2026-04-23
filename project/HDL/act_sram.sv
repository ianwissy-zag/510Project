`timescale 1ns / 1ps

// Simple dual-port synchronous SRAM for activation ping-pong buffers.
//
// Separate read and write ports allow concurrent operation: while the
// systolic array reads the current activation buffer, the AXI interface
// can simultaneously fill the opposite (ping-pong) buffer.
// The ping-pong selection is managed externally via the write enables in
// axis_to_pingpong_buffer; this module is instantiated once per buffer.
module act_sram #(
    parameter DATA_WIDTH = 512,         // one row: 32 × BF16 elements
    parameter DEPTH      = 32,          // rows to fill the 32×32 array
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic                    clk,
    // Write port – driven by AXI stream
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   waddr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    // Read port – driven by systolic array controller
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
