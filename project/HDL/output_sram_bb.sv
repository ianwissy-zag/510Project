// Blackbox stub for output_sram (partial-sum result buffer).
// In the physical flow this is an OpenRAM-generated hardened macro.
/* verilator lint_off DECLFILENAME */
/* verilator lint_off TIMESCALEMOD */
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNDRIVEN */
(* blackbox *)
module output_sram #(
    parameter PSUM_WIDTH = 32,
    parameter ARRAY_SIZE = 32,
    parameter DATA_WIDTH = PSUM_WIDTH * ARRAY_SIZE,
    parameter DEPTH      = 32,
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic                  clk,
    input  logic                  we,
    input  logic [ADDR_WIDTH-1:0] waddr,
    input  logic [DATA_WIDTH-1:0] wdata,
    input  logic                  re,
    input  logic [ADDR_WIDTH-1:0] raddr,
    output logic [DATA_WIDTH-1:0] rdata
);
endmodule
