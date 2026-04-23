// This code was written by Gemini 3 Flash

module mac (
    input  wire        clk,   // 1-bit clock
    input  wire        rst,   // 1-bit synchronous active high reset
    input  wire [7:0]  a,     // 8-bit signed input a
    input  wire [7:0]  b,     // 8-bit signed input b
    output reg  [31:0] out    // 32-bit signed accumulator output
);

    // Internal wire to hold the product of a and b
    // 8-bit signed * 8-bit signed results in a 16-bit signed product
    wire signed [15:0] product;

    // Use signed keyword to ensure correct arithmetic shift and sign extension
    assign product = $signed(a) * $signed(b);

    always @(posedge clk) begin
        if (rst) begin
            // Synchronous reset: Clear the accumulator
            out <= 32'sd0;
        end else begin
            // Accumulate: Previous output + current product
            // Verilog handles sign extension of 'product' to 32 bits automatically 
            // when adding to the 32-bit 'out' register.
            out <= $signed(out) + $signed(product);
        end
    end

endmodule
