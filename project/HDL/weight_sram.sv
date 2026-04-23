`timescale 1ns / 1ps

// Single-port synchronous SRAM for weight storage.
//
// Write and compute phases are non-overlapping (AXI writes the full weight
// matrix before the systolic array starts reading), so a single address bus
// shared between the two phases is sufficient.
//
// Read-before-write: rdata reflects mem[addr] before a same-cycle write.
// DEPTH = 32 holds one weight tile (ARRAY_SIZE rows × 512 bits).
// The host streams a fresh tile before each start pulse.
module weight_sram #(
    parameter DATA_WIDTH = 512,          // one row: 32 × BF16 elements
    parameter DEPTH      = 32,           // one tile: ARRAY_SIZE rows
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
