`timescale 1ns / 1ps

module crossbar_mac #(
    parameter int INPUT_WIDTH = 8,
    // Add 2 bits to output width to prevent overflow when summing 4 rows (log2(4) = 2)
    parameter int OUTPUT_WIDTH = INPUT_WIDTH + 2
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

    // Intermediate array to hold the result of activation * weight at each cross-point
    // Note: We use INPUT_WIDTH + 1 bits here. If an 8-bit input is -128, 
    // multiplying it by -1 gives +128, which requires 9 bits to represent in signed logic.
    logic signed [INPUT_WIDTH:0] mult_results [3:0][3:0];

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
    // 2. Combinational Accumulation Stage (Column summation)
    // ==============================================================================
    logic signed [OUTPUT_WIDTH-1:0] col_sum [3:0];

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
    // ==============================================================================
    always_ff @(posedge clk) begin
        if (rst) begin
            for (int i = 0; i < 4; i++)
                mac_outs_o[i] <= '0;
        end else begin
            for (int i = 0; i < 4; i++)
                mac_outs_o[i] <= col_sum[i];
        end
    end

endmodule
