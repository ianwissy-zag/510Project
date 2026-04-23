// Blackbox stub for weight_sram.
// Preserves the full parameter and port interface so Verilator can resolve
// the instantiation in top.sv.  The empty body causes Yosys to treat this
// as a blackbox cell — no internal logic is synthesized.
// The production implementation is an OpenRAM-generated SRAM macro.
/* verilator lint_off DECLFILENAME */
/* verilator lint_off TIMESCALEMOD */
/* verilator lint_off UNUSEDSIGNAL */
/* verilator lint_off UNDRIVEN */
(* blackbox *)
module weight_sram #(
    parameter DATA_WIDTH = 512,
    parameter DEPTH      = 32,
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic                    clk,
    input  logic                    we,
    input  logic [ADDR_WIDTH-1:0]   addr,
    input  logic [DATA_WIDTH-1:0]   wdata,
    output logic [DATA_WIDTH-1:0]   rdata
);
endmodule
