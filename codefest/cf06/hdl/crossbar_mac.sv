`timescale 1ns / 1ps

module crossbar_mac #(
    parameter int INPUT_WIDTH  = 8,
    // Output is same width as input — results wrap on overflow (modular arithmetic).
    parameter int OUTPUT_WIDTH = INPUT_WIDTH
)(
    input  logic clk,
    input  logic rst,   // synchronous active-high reset

    // 4 input activations (signed)
    input  logic signed [INPUT_WIDTH-1:0] acts_i [3:0],

    // 4x4 weight matrix: weights_i[row][col]
    // Encoding: 1'b0 = +1, 1'b1 = -1
    input  logic [3:0][3:0] weights_i,

    // 4 output column accumulations (signed, registered)
    output logic signed [OUTPUT_WIDTH-1:0] mac_outs_o [3:0]
);

    // Intermediate multiply results: INPUT_WIDTH+1 bits so that negating -128
    // produces +128 without overflow (9 bits for INPUT_WIDTH=8).
    logic signed [INPUT_WIDTH:0] mult_results [3:0][3:0];

    // Column sums: wide enough to hold 4 × (INPUT_WIDTH+1)-bit values exactly
    // before truncation to OUTPUT_WIDTH.  For INPUT_WIDTH=8: 11 bits covers
    // the full range [-512, 512].  The lower OUTPUT_WIDTH bits are then
    // captured in the output register, producing natural 8-bit wrap-around.
    logic signed [INPUT_WIDTH+2:0] col_sum [3:0];

    // ==============================================================================
    // 1. Parallel Multiplication Stage (Cross-point intersections)
    // ==============================================================================
    genvar row, col;
    generate
        for (col = 0; col < 4; col++) begin : gen_cols
            for (row = 0; row < 4; row++) begin : gen_rows
                always_comb begin
                    if (weights_i[row][col] == 1'b0) begin
                        // Weight is +1: Pass activation through directly
                        mult_results[row][col] = acts_i[row];
                    end else begin
                        // Weight is -1: Negate activation (Two's complement)
                        mult_results[row][col] = -acts_i[row];
                    end
                end
            end
        end
    endgenerate

    // ==============================================================================
    // 2. Combinational Accumulation Stage (Column summation — full precision)
    // ==============================================================================
    genvar c;
    generate
        for (c = 0; c < 4; c++) begin : gen_accum
            always_comb begin
                col_sum[c] = mult_results[0][c] +
                             mult_results[1][c] +
                             mult_results[2][c] +
                             mult_results[3][c];
            end
        end
    endgenerate

    // ==============================================================================
    // 3. Output Register with Synchronous Reset
    //    Truncates col_sum to OUTPUT_WIDTH bits — overflow wraps (modular).
    // ==============================================================================
    always_ff @(posedge clk) begin
        if (rst) begin
            for (int i = 0; i < 4; i++)
                mac_outs_o[i] <= '0;
        end else begin
            for (int i = 0; i < 4; i++)
                mac_outs_o[i] <= col_sum[i][OUTPUT_WIDTH-1:0];
        end
    end

endmodule
