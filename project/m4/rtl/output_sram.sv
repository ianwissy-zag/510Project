`timescale 1ns / 1ps

// Output SRAM — combinational read, registered write.
// Stores COLS FP32 partial sums. Combinational read ensures the readback
// module gets valid data immediately after the write completes.

module output_sram #(
    parameter COLS       = 32,
    parameter PSUM_WIDTH = 32
)(
    input  logic                        clk,
    input  logic                        we,
    input  logic [COLS*PSUM_WIDTH-1:0]  wdata,
    output logic [COLS*PSUM_WIDTH-1:0]  rdata
);
    logic [COLS*PSUM_WIDTH-1:0] mem;

    always_ff @(posedge clk)
        if (we) mem <= wdata;

    assign rdata = mem;   // combinational
endmodule
