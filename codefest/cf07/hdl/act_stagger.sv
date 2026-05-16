`timescale 1ns / 1ps

// Triangular activation stagger buffer for classical systolic dataflow.
//
// Row r receives its activation delayed by r+1 clock cycles so that at
// cycle t of the STREAM phase, PE[r] processes M row m = t - r - 1.
// With all ROWS PEs in flight simultaneously, every PE does useful work
// every clock: 100% MAC utilization, no psum-wavefront redundancy.
//
// Stage layout: stage[r][d] holds act_in[r] from d+1 cycles ago.
// act_out[r] = stage[r][r]  →  delay of r+1 cycles.
//
// en=0 freezes the buffer (used during LOAD_WT).
// Shifting in 0 during drain cycles lets the last M row propagate correctly.

module act_stagger #(
    parameter ROWS      = 16,
    parameter ACT_WIDTH = 16
)(
    input  logic clk,
    input  logic rst_n,
    input  logic en,
    input  logic [ROWS-1:0][ACT_WIDTH-1:0] act_in,
    output logic [ROWS-1:0][ACT_WIDTH-1:0] act_out
);
    logic [ROWS-1:0][ROWS-1:0][ACT_WIDTH-1:0] stage;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (int r = 0; r < ROWS; r++)
                for (int d = 0; d < ROWS; d++)
                    stage[r][d] <= '0;
        end else if (en) begin
            for (int r = 0; r < ROWS; r++) begin
                stage[r][0] <= act_in[r];
                for (int d = 1; d < ROWS; d++)
                    stage[r][d] <= stage[r][d-1];
            end
        end
    end

    genvar r;
    generate
        for (r = 0; r < ROWS; r++)
            assign act_out[r] = stage[r][r];
    endgenerate
endmodule
