// Blackbox stub for systolic_array_32x32.
// Keeps the packed-2D port interface so Yosys resolves the instantiation in
// top.sv without synthesizing 1024 PEs — the array is too large to fit in
// the available RAM during the OpenLane synthesis step.
/* verilator lint_off DECLFILENAME */
/* verilator lint_off TIMESCALEMOD */
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNDRIVEN */
(* blackbox *)
module systolic_array_32x32 #(
    parameter ARRAY_SIZE = 32,
    parameter ACT_WIDTH  = 16,
    parameter WT_WIDTH   = 16,
    parameter PSUM_WIDTH = 32
)(
    input  logic clk,
    input  logic rst_n,
    input  logic load_wt,
    input  logic [ARRAY_SIZE-1:0][ACT_WIDTH-1:0]  act_in,
    input  logic [ARRAY_SIZE-1:0][WT_WIDTH-1:0]   wt_in,
    input  logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_in,
    output logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_out
);
endmodule
