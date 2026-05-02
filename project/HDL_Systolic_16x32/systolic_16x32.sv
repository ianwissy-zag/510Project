`timescale 1ns / 1ps

// 16-row × 32-column BF16 weight-stationary systolic array.
// Broadcast activation topology: act_in[r] is broadcast to ALL columns
// in row r simultaneously — no horizontal propagation delay.
//
// During LOAD_WT (2×ROWS = 32 cycles):
//   wt_in[col] enters PE[0][col] and shifts down. The 2-register depth
//   (weight_reg + w_out) requires 2×ROWS cycles to fully fill all rows.
//
// During COMPUTE (ROWS = 16 cycles):
//   psum flows top-to-bottom. All PEs in a row fire simultaneously.
//   After ROWS cycles, PE[ROWS-1][col] has the full K_DEPTH inner product.
//
// Reset: asynchronous active-low.

module systolic_16x32 #(
    parameter ROWS   = 16,
    parameter COLS   = 32,
    parameter ACT_W  = 16,
    parameter WT_W   = 16,
    parameter PSUM_W = 32
)(
    input  logic clk,
    input  logic rst_n,
    input  logic load_wt,

    input  logic [COLS-1:0][WT_W-1:0]   wt_in,    // one BF16 per column (top of shift chain)
    input  logic [ROWS-1:0][ACT_W-1:0]  act_in,   // one BF16 per row, broadcast to all cols
    input  logic [COLS-1:0][PSUM_W-1:0] psum_in,  // FP32 entering top of each column
    output logic [COLS-1:0][PSUM_W-1:0] psum_out  // FP32 exiting bottom of each column
);
    // psum wires flow top→bottom; wt wires flow top→bottom (during load)
    logic [ROWS:0][COLS-1:0][PSUM_W-1:0] p_wire;
    logic [ROWS:0][COLS-1:0][WT_W-1:0]   w_wire;

    genvar c;
    generate
        for (c = 0; c < COLS; c++) begin : g_col_in
            assign p_wire[0][c] = psum_in[c];
            assign w_wire[0][c] = wt_in[c];
        end
    endgenerate

    genvar r;
    generate
        for (r = 0; r < ROWS; r++) begin : g_row
            for (c = 0; c < COLS; c++) begin : g_col
                pe u_pe (
                    .clk     (clk),
                    .rst_n   (rst_n),
                    .load_wt (load_wt),
                    .a_in    (act_in[r]),        // broadcast: same act to all cols in row r
                    .w_in    (w_wire[r][c]),
                    .psum_in (p_wire[r][c]),
                    .a_out   (),                 // unused in broadcast topology
                    .w_out   (w_wire[r+1][c]),
                    .psum_out(p_wire[r+1][c])
                );
            end
        end
    endgenerate

    generate
        for (c = 0; c < COLS; c++) begin : g_col_out
            assign psum_out[c] = p_wire[ROWS][c];
        end
    endgenerate

endmodule
