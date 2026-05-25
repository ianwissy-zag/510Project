`timescale 1ns / 1ps

// Activation SRAM — combinational read, registered write.
// Stores one vector of ROWS BF16 values (no address — single slot).

module act_sram #(
    parameter ROWS      = 16,
    parameter ACT_WIDTH = 16
)(
    input  logic                      clk,
    input  logic                      we,
    input  logic [ROWS*ACT_WIDTH-1:0] wdata,
    output logic [ROWS*ACT_WIDTH-1:0] rdata
);
    logic [ROWS*ACT_WIDTH-1:0] mem;

    always_ff @(posedge clk)
        if (we) mem <= wdata;

    assign rdata = mem;   // combinational — always valid
endmodule
