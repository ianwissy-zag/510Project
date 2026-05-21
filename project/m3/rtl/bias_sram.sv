`timescale 1ns / 1ps

// bias_sram — holds COLS FP32 bias values for the current N-tile.
//
// Loaded via two 512-bit AXI beats (16 FP32 values per beat):
//   beat_sel=0 → bias[0..15]
//   beat_sel=1 → bias[16..31]
// All values are read combinationally in parallel during readback.

module bias_sram #(
    parameter COLS       = 32,
    parameter DATA_WIDTH = 32    // FP32
)(
    input  logic clk,
    input  logic we,
    input  logic beat_sel,                              // 0=low half, 1=high half
    input  logic [(COLS/2)*DATA_WIDTH-1:0] wdata,       // 16 FP32 per beat
    output logic [COLS-1:0][DATA_WIDTH-1:0] rdata
);
    localparam HALF = COLS / 2;

    logic [COLS-1:0][DATA_WIDTH-1:0] mem;

    always_ff @(posedge clk) begin
        if (we) begin
            for (int i = 0; i < HALF; i++) begin
                if (!beat_sel)
                    mem[i]        <= wdata[i*DATA_WIDTH +: DATA_WIDTH];
                else
                    mem[HALF + i] <= wdata[i*DATA_WIDTH +: DATA_WIDTH];
            end
        end
    end

    assign rdata = mem;
endmodule
