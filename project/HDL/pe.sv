`timescale 1ns / 1ps
/* verilator lint_off DECLFILENAME */
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
    logic [WT_WIDTH-1:0]   weight_reg;
    logic [PSUM_WIDTH-1:0] mac_result;

    bf16_mac_unit u_mac (
        .act_bf16    (a_in),
        .wt_bf16     (weight_reg),
        .acc_fp32_in (psum_in),
        .acc_fp32_out(mac_result)
    );

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            weight_reg <= '0;
            a_out      <= '0;
            w_out      <= '0;
            psum_out   <= '0;
        end else begin
            if (load_wt) begin
                weight_reg <= w_in;
                w_out      <= weight_reg;
            end else begin
                a_out    <= a_in;
                psum_out <= mac_result;
            end
        end
    end

endmodule
