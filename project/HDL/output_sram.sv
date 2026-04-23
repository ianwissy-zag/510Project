`timescale 1ns / 1ps

// Dual-port output SRAM that buffers partial-sum results from the systolic array.
//
// Write port: accepts all 32 psum_out values in a single 1024-bit word
//             (ARRAY_SIZE × PSUM_WIDTH), written one row per cycle.
// Read port:  returns the same 1024-bit row to the AXI read-back path,
//             which is responsible for serialising it into AXI-S beats.
//
// Separate read/write ports allow the systolic array to write new results
// while the read-back interface drains the previous tile.
module output_sram #(
    parameter PSUM_WIDTH  = 32,
    parameter ARRAY_SIZE  = 32,
    parameter DATA_WIDTH  = PSUM_WIDTH * ARRAY_SIZE, // 1024 bits per row
    parameter DEPTH       = 32,
    parameter ADDR_WIDTH  = $clog2(DEPTH)
)(
    input  logic                    clk,
    // Write port – driven by systolic array bottom edge
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   waddr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    // Read port – driven by AXI read-back interface
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
