`timescale 1ns / 1ps

module mac_pe #(
    parameter ACT_WIDTH = 16,   // BF16
    parameter WT_WIDTH  = 16,   // BF16
    parameter PSUM_WIDTH = 32   // FP32
)(
    input  logic clk,
    input  logic rst_n,
    
    // Control
    input  logic load_wt,       // High to shift weights in, Low to compute
    
    // Data Inputs
    input  logic [ACT_WIDTH-1:0]  a_in,    // Activation from left PE
    input  logic [WT_WIDTH-1:0]   w_in,    // Weight from top PE (for loading)
    input  logic [PSUM_WIDTH-1:0] psum_in, // Partial sum from top PE
    
    // Data Outputs
    output logic [ACT_WIDTH-1:0]  a_out,   // Activation to right PE
    output logic [WT_WIDTH-1:0]   w_out,   // Weight to bottom PE (for loading)
    output logic [PSUM_WIDTH-1:0] psum_out // Partial sum to bottom PE
);

    // Internal weight register
    logic [WT_WIDTH-1:0] weight_reg;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            weight_reg <= '0;
            a_out      <= '0;
            w_out      <= '0;
            psum_out   <= '0;
        end else begin
            if (load_wt) begin
                // Shift phase: weights flow top-to-bottom
                weight_reg <= w_in;
                w_out      <= weight_reg; // Pass previous weight down
            end else begin
                // Compute phase: pipeline the activation
                a_out <= a_in;
                
                // ---------------------------------------------------------
                // TODO: REPLACE WITH ACTUAL BF16/FP32 IP CORES
                // ---------------------------------------------------------
                // Logically: psum_out <= psum_in + (a_in * weight_reg);
                // The below is a placeholder that will synthesize as integer math.
                // You must instantiate a BF16 multiplier and FP32 adder here.
                psum_out <= psum_in + (a_in * weight_reg); 
                // ---------------------------------------------------------
            end
        end
    end

endmodule
