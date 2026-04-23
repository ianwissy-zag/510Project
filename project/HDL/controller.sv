`timescale 1ns / 1ps

// Tile sequencer for the 32×32 weight-stationary systolic array.
//
// One "tile" = load weights from weight_sram into all PEs, then feed one
// activation vector and wait for the pipeline to drain, then capture
// psum_out into output_sram[0].
//
// Weight address schedule during LOAD_WT:
//   The PE column is a 2-register shift chain (weight_reg → w_out → next row).
//   To land W[r] in PE row r after 64 clock edges, the SRAM address must
//   descend 31→0 every two cycles so that the data arriving at PE[0] on
//   cycle t ends up at PE[r] = (63-t)/2 rows below after the remaining hops.
//   Address driven at counter t  →  wt_addr = 31 − (t >> 1).
//   With a one-cycle registered SRAM output, the data reaching PE[0]'s
//   weight_reg at posedge (t+1) came from SRAM[31 − (t>>1)].
//   After the full 64 cycles PE[r].weight_reg = SRAM[r] for r = 0..31.
//
// Activation:
//   act_sram row 0 is the 32-element input vector (one token).
//   Re is asserted during LOAD_WT so data is ready on the first COMPUTE cycle.
//   act_in[c] is held constant for the full COMPUTE phase (2*N = 64 cycles)
//   so every PE column sees the correct activation value when the running
//   partial sum finally reaches it.
//
// Capture:
//   psum_out is sampled one cycle after the last COMPUTE clock edge.
//   Written as a 1024-bit word to output_sram row 0.
module controller #(
    parameter ARRAY_SIZE     = 32,
    parameter ACT_WIDTH      = 16,
    parameter WT_WIDTH       = 16,
    parameter PSUM_WIDTH     = 32,
    parameter WT_ADDR_WIDTH  = $clog2(ARRAY_SIZE),
    parameter ACT_ADDR_WIDTH = 5,    // $clog2(32)
    parameter OUT_ADDR_WIDTH = 5     // $clog2(32)
)(
    input  logic clk,
    input  logic rst_n,

    // Handshake
    input  logic start,
    input  logic act_buf_sel,  // 0 = ping (act_sram_0), 1 = pong (act_sram_1)
    input  logic first_tile,   // 1 = clear accumulator (first K tile)
    input  logic last_tile,    // 1 = write output SRAM (last K tile)
    output logic done,

    // Weight SRAM – read-only during LOAD_WT (we=0, driven externally)
    output logic [WT_ADDR_WIDTH-1:0]          wt_addr,
    input  logic [ARRAY_SIZE*WT_WIDTH-1:0]    wt_rdata,  // 512-bit registered

    // Activation SRAMs (ping / pong)
    output logic                              act_re_0,
    output logic                              act_re_1,
    output logic [ACT_ADDR_WIDTH-1:0]         act_raddr,
    input  logic [ARRAY_SIZE*ACT_WIDTH-1:0]   act_rdata_0,
    input  logic [ARRAY_SIZE*ACT_WIDTH-1:0]   act_rdata_1,

    // Systolic array ports driven by controller
    output logic                                   load_wt,
    output logic [ARRAY_SIZE-1:0][ACT_WIDTH-1:0]  act_in,
    output logic [ARRAY_SIZE-1:0][WT_WIDTH-1:0]   wt_in,
    output logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_in,
    input  logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_out,

    // Output SRAM write port
    output logic                              out_we,
    output logic [OUT_ADDR_WIDTH-1:0]         out_waddr,
    output logic [ARRAY_SIZE*PSUM_WIDTH-1:0]  out_wdata  // 1024-bit
);

    localparam int LOAD_CYCLES    = 2 * ARRAY_SIZE;  // 64
    localparam int COMPUTE_CYCLES = 2 * ARRAY_SIZE;  // 64

    typedef enum logic [2:0] {
        IDLE, LOAD_WT, COMPUTE, CAPTURE, DONE_ST
    } state_t;

    state_t       state;
    logic [6:0]   counter;   // 0..127, covers max(64,64)
    logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] accum_reg; // inter-tile accumulator

    // ── State machine ────────────────────────────────────────────────────
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state   <= IDLE;
            counter <= '0;
        end else begin
            case (state)
                IDLE:
                    if (start) begin
                        state   <= LOAD_WT;
                        counter <= '0;
                    end

                LOAD_WT:
                    if (counter == 7'(LOAD_CYCLES - 1)) begin
                        state   <= COMPUTE;
                        counter <= '0;
                    end else
                        counter <= counter + 1'b1;

                COMPUTE:
                    if (counter == 7'(COMPUTE_CYCLES - 1)) begin
                        state   <= CAPTURE;
                        counter <= '0;
                    end else
                        counter <= counter + 1'b1;

                CAPTURE:  begin state <= DONE_ST; counter <= '0; end
                DONE_ST:  begin state <= IDLE;    counter <= '0; end
                default:        state <= IDLE;
            endcase
        end
    end

    // ── Inter-tile accumulator ───────────────────────────────────────────
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            accum_reg <= '0;
        else if (state == CAPTURE)
            accum_reg <= psum_out;
    end

    // ── Weight SRAM address ──────────────────────────────────────────────
    // Descend 31→0 every two cycles during LOAD_WT.
    assign wt_addr = (state == LOAD_WT)
                   ? WT_ADDR_WIDTH'(ARRAY_SIZE - 1) - WT_ADDR_WIDTH'(counter[6:1])
                   : '0;

    // ── Activation SRAM ──────────────────────────────────────────────────
    // Assert re during LOAD_WT too so data is ready on first COMPUTE cycle.
    assign act_raddr = '0;
    assign act_re_0  = ((state == LOAD_WT) || (state == COMPUTE)) && !act_buf_sel;
    assign act_re_1  = ((state == LOAD_WT) || (state == COMPUTE)) &&  act_buf_sel;

    // ── Systolic array control ────────────────────────────────────────────
    assign load_wt = (state == LOAD_WT);

    // wt_in: unpack 512-bit SRAM word into 32 × WT_WIDTH elements
    genvar c;
    generate
        for (c = 0; c < ARRAY_SIZE; c++) begin : g_wt_in
            assign wt_in[c] = (state == LOAD_WT)
                             ? wt_rdata[c * WT_WIDTH +: WT_WIDTH]
                             : '0;
        end
    endgenerate

    // act_in: unpack 512-bit SRAM word into 32 × ACT_WIDTH elements
    logic [ARRAY_SIZE*ACT_WIDTH-1:0] act_rdata_sel;
    assign act_rdata_sel = act_buf_sel ? act_rdata_1 : act_rdata_0;

    generate
        for (c = 0; c < ARRAY_SIZE; c++) begin : g_act_in
            assign act_in[c] = (state == COMPUTE)
                              ? act_rdata_sel[c * ACT_WIDTH +: ACT_WIDTH]
                              : '0;
        end
    endgenerate

    // psum_in: zero on first tile, accumulated partial sum on subsequent tiles
    generate
        for (c = 0; c < ARRAY_SIZE; c++) begin : g_psum_in
            assign psum_in[c] = first_tile ? '0 : accum_reg[c];
        end
    endgenerate

    // ── Output SRAM capture ───────────────────────────────────────────────
    assign out_we    = (state == CAPTURE) && last_tile;
    assign out_waddr = '0;

    genvar p;
    generate
        for (p = 0; p < ARRAY_SIZE; p++) begin : g_psum_pack
            assign out_wdata[p * PSUM_WIDTH +: PSUM_WIDTH] = psum_out[p];
        end
    endgenerate

    // ── Done pulse ────────────────────────────────────────────────────────
    assign done = (state == DONE_ST);

endmodule
