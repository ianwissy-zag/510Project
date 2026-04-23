// Blackbox stub for act_sram (activation ping-pong buffers).
// In the physical flow these are OpenRAM-generated hardened macros.
/* verilator lint_off DECLFILENAME */
/* verilator lint_off TIMESCALEMOD */
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNDRIVEN */
(* blackbox *)
module act_sram #(
    parameter DATA_WIDTH = 512,
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
