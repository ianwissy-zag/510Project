`timescale 1ns / 1ps

// Controller for the 16×32 BF16 systolic accelerator.
//
// Supports forward pass and backward dinp pass via the mode input.
// In both modes, the dataflow is identical — only the weight SRAM selected differs.
//
// State machine: IDLE → LOAD_WT → COMPUTE → CAPTURE → DONE
//
//   LOAD_WT (ROWS = 16 cycles): w_out is combinational so one cycle per row suffices.
//   COMPUTE (ROWS = 16 cycles): broadcast act_in, psum propagates downward.
//   CAPTURE: stores psum_out into accumulator or output SRAM.
//   DONE: pulses done for one cycle.
//
// K-tiling: LOAD_WT + COMPUTE repeats for each K-tile.
//   first_tile=1: psum_in seeded from zero.
//   last_tile=1:  output written to output_sram on CAPTURE.
//
// Reset: asynchronous active-low.

module controller #(
    parameter ROWS        = 16,
    parameter COLS        = 32,
    parameter ACT_WIDTH   = 16,
    parameter WT_WIDTH    = 16,
    parameter PSUM_WIDTH  = 32,
    parameter ADDR_WIDTH  = $clog2(ROWS)
)(
    input  logic clk,
    input  logic rst_n,

    // Host control
    input  logic start,
    input  logic mode,         // 0=forward, 1=backward dinp
    input  logic first_tile,   // seed accumulators from zero
    input  logic last_tile,    // write result to output SRAM
    input  logic act_buf_sel,  // 0=ping, 1=pong
    input  logic fwd_buf_sel,  // forward weight bank: 0=ping, 1=pong
    input  logic bwd_buf_sel,  // backward weight bank: 0=ping, 1=pong
    output logic done,

    // Weight SRAM read (forward ping/pong)
    output logic [ADDR_WIDTH-1:0]      fwd_wt_addr_0, fwd_wt_addr_1,
    output logic                       fwd_wt_re_0,   fwd_wt_re_1,
    input  logic [COLS*WT_WIDTH-1:0]   fwd_wt_rdata_0, fwd_wt_rdata_1,

    // Weight SRAM read (backward ping/pong)
    output logic [ADDR_WIDTH-1:0]      bwd_wt_addr_0, bwd_wt_addr_1,
    output logic                       bwd_wt_re_0,   bwd_wt_re_1,
    input  logic [COLS*WT_WIDTH-1:0]   bwd_wt_rdata_0, bwd_wt_rdata_1,

    // Activation SRAM read
    output logic                       act_re_0, act_re_1,
    input  logic [ROWS*ACT_WIDTH-1:0]  act_rdata_0, act_rdata_1,

    // Systolic array control
    output logic                           load_wt,
    output logic [COLS-1:0][WT_WIDTH-1:0]  wt_in,
    output logic [ROWS-1:0][ACT_WIDTH-1:0] act_in,
    output logic [COLS-1:0][PSUM_WIDTH-1:0] psum_in,
    input  logic [COLS-1:0][PSUM_WIDTH-1:0] psum_out,

    // Output SRAM write
    output logic                        out_we,
    output logic [COLS*PSUM_WIDTH-1:0]  out_wdata
);
    // State machine
    typedef enum logic [2:0] {
        IDLE, LOAD_WT, COMPUTE, CAPTURE, DONE_ST
    } state_t;

    state_t state;
    logic [ADDR_WIDTH-1:0] counter;  // LOAD_WT and COMPUTE both need up to ROWS-1 = 15

    // Accumulator register (FP32, one per column)
    logic [COLS-1:0][PSUM_WIDTH-1:0] accum_reg;

    // Selected weight data
    logic [COLS*WT_WIDTH-1:0] wt_rdata_mux;
    always_comb begin
        if (!mode) // forward
            wt_rdata_mux = fwd_buf_sel ? fwd_wt_rdata_1 : fwd_wt_rdata_0;
        else       // backward
            wt_rdata_mux = bwd_buf_sel ? bwd_wt_rdata_1 : bwd_wt_rdata_0;
    end

    // Selected activation data
    logic [ROWS*ACT_WIDTH-1:0] act_rdata_mux;
    assign act_rdata_mux = act_buf_sel ? act_rdata_1 : act_rdata_0;

    // Unpack wt_rdata_mux to per-column format
    genvar c;
    generate
        for (c = 0; c < COLS; c++) begin : g_wt_unpack
            assign wt_in[c] = wt_rdata_mux[c*WT_WIDTH +: WT_WIDTH];
        end
    endgenerate

    // Unpack act_rdata_mux to per-row format
    genvar r;
    generate
        for (r = 0; r < ROWS; r++) begin : g_act_unpack
            assign act_in[r] = act_rdata_mux[r*ACT_WIDTH +: ACT_WIDTH];
        end
    endgenerate

    // psum_in: zero on first tile, accumulator on subsequent tiles
    generate
        for (c = 0; c < COLS; c++) begin : g_psum_in
            assign psum_in[c] = first_tile ? '0 : accum_reg[c];
        end
    endgenerate

    // Weight SRAM address: one row per cycle, direct counter mapping.
    assign fwd_wt_addr_0 = counter;
    assign fwd_wt_addr_1 = counter;
    assign bwd_wt_addr_0 = counter;
    assign bwd_wt_addr_1 = counter;

    logic in_load;
    assign in_load = (state == LOAD_WT);

    assign fwd_wt_re_0 = in_load && !mode && !fwd_buf_sel;
    assign fwd_wt_re_1 = in_load && !mode &&  fwd_buf_sel;
    assign bwd_wt_re_0 = in_load &&  mode && !bwd_buf_sel;
    assign bwd_wt_re_1 = in_load &&  mode &&  bwd_buf_sel;
    assign act_re_0    = (state == COMPUTE) && !act_buf_sel;
    assign act_re_1    = (state == COMPUTE) &&  act_buf_sel;

    assign load_wt  = (state == LOAD_WT);
    assign done     = (state == DONE_ST);
    assign out_we   = (state == CAPTURE) && last_tile;
    assign out_wdata = {COLS{1'b0}};   // overridden below

    // Pack psum_out for output SRAM write
    logic [COLS*PSUM_WIDTH-1:0] psum_out_flat;
    generate
        for (c = 0; c < COLS; c++) begin : g_psum_flat
            assign psum_out_flat[c*PSUM_WIDTH +: PSUM_WIDTH] = psum_out[c];
        end
    endgenerate

    // State machine
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state   <= IDLE;
            counter <= '0;
            accum_reg <= '0;
        end else begin
            case (state)
                IDLE: begin
                    if (start) begin
                        state   <= LOAD_WT;
                        counter <= '0;
                    end
                end

                LOAD_WT: begin
                    if (counter == ADDR_WIDTH'(ROWS - 1)) begin
                        state   <= COMPUTE;
                        counter <= '0;
                    end else
                        counter <= counter + 1'b1;
                end

                COMPUTE: begin
                    if (counter == ROWS[$clog2(ROWS):0] - 1) begin
                        state   <= CAPTURE;
                        counter <= '0;
                    end else
                        counter <= counter + 1'b1;
                end

                CAPTURE: begin
                    // Store psum_out into accumulator
                    for (int i = 0; i < COLS; i++)
                        accum_reg[i] <= psum_out[i];
                    state <= DONE_ST;
                end

                DONE_ST:
                    state <= IDLE;

                default: state <= IDLE;
            endcase
        end
    end

    // Output SRAM write data (driven combinationally from psum_out on CAPTURE)
    assign out_wdata = (state == CAPTURE) ? psum_out_flat : '0;

endmodule
