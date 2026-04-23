// This code was written by Google Gemini 3.1 Pro

module mac (
    input  wire               clk,
    input  wire               rst,
    input  wire signed [7:0]  a,
    input  wire signed [7:0]  b,
    output reg  signed [31:0] out
);

    always @(posedge clk) begin
        if (rst) begin
            out <= 32'sd0;
        end else begin
            out <= out + (a * b);
        end
    end

endmodule
