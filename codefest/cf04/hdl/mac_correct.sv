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
            // We explicitly sign-extend the 16-bit product to 32 bits.
            // This tells Verilator: "I know this is 16 bits, and I want it to be 32."
            out <= out + {{16{product[15]}}, product};
        end
    end

endmodule
