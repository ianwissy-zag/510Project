`timescale 1ns / 1ps

// Processing element for the 16×16 BF16 systolic array.
//
// Weight-stationary: weight_reg holds the BF16 weight for this PE.
// During LOAD_WT: weights shift top-to-bottom through w_in/w_out.
// During COMPUTE: psum_in accumulates the BF16 MAC result top-to-bottom.
//
// Reset: asynchronous active-low.
//
// Ports:
//   clk      IN   Clock
//   rst_n    IN   Async active-low reset
//   load_wt  IN   High: shift weights; Low: compute
//   a_in     IN  [15:0] BF16 activation from left neighbour
//   w_in     IN  [15:0] BF16 weight from above (LOAD_WT chain)
//   psum_in  IN  [31:0] FP32 partial sum from above
//   a_out    OUT [15:0] BF16 activation to right neighbour
//   w_out    OUT [15:0] BF16 weight downward (LOAD_WT chain)
//   psum_out OUT [31:0] FP32 partial sum downward

module pe (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        load_wt,
    input  logic [15:0] a_in,
    input  logic [15:0] w_in,
    input  logic [31:0] psum_in,
    output logic [15:0] a_out,
    output logic [15:0] w_out,
    output logic [31:0] psum_out
);
    logic [15:0] weight_reg;
    logic [31:0] mac_result;

    // w_out is combinational so the shift chain needs only ROWS cycles, not 2×ROWS.
    assign w_out = weight_reg;

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
            psum_out   <= '0;
        end else if (load_wt) begin
            weight_reg <= w_in;
        end else begin
            a_out    <= a_in;
            psum_out <= mac_result;
        end
    end
endmodule
