`timescale 1ns / 1ps

// Controller for the 32×32 BF16 systolic accelerator — streaming M-row mode.
//
// State machine: IDLE → LOAD_WT → STREAM → DONE
//   (IDLE → STREAM directly when wt_preloaded_r is set from a prior drain-overlap)
//
//   LOAD_WT (ROWS cycles): reads weight SRAM row by row, shifts into PEs.
//
//   STREAM (M_count + ROWS cycles):
//     Each cycle the act_stagger module receives one new activation slice
//     (ROWS BF16 values — one K element per row) from the AXI bus.
//     Row r of the stagger delays its input by r+1 cycles, so at stream
//     cycle t PE[r] processes M row (t - r - 1).  After ROWS fill cycles,
//     PE[ROWS-1] produces a valid result every cycle.
//
//     Accumulation across K-tiles:
//       psum_in[0][c] = 0            (first_k_tile = 1)
//       psum_in[0][c] = accum[m][c]  (subsequent K-tiles)
//     The PEs add each K-tile's contribution inside the systolic array, so
//     accum_wdata = psum_out (no separate FP32 adder needed in the controller).
//
//   act_in to the systolic array is driven by the act_stagger module output,
//   wired externally in top.sv — the controller does not drive act_in.
//
// Reset: asynchronous active-low.

module controller #(
    parameter ROWS        = 16,
    parameter COLS        = 32,
    parameter WT_WIDTH    = 16,
    parameter PSUM_WIDTH  = 32,
    parameter M_MAX       = 256,
    parameter ADDR_WIDTH  = $clog2(ROWS),
    parameter M_ADDR_W    = $clog2(M_MAX + 1)
)(
    input  logic clk,
    input  logic rst_n,

    // Host control
    input  logic                  start,
    input  logic                  mode,         // 0=forward, 1=backward dinp
    input  logic                  first_k_tile, // 1→seed psum from zero
    input  logic                  buf_sel,      // selects ping (0) or pong (1) weight bank
    input  logic [M_ADDR_W-1:0]   M_count,      // number of M rows to stream
    input  logic                  preload_rdy,  // 1→ next tile weights in ~buf_sel SRAM; start drain-overlap LOAD_WT
    output logic                  done,

    // Weight SRAM read (shared ping/pong for forward and backward)
    output logic [ADDR_WIDTH-1:0]      wt_addr_0, wt_addr_1,
    output logic                       wt_re_0,   wt_re_1,
    input  logic [COLS*WT_WIDTH-1:0]   wt_rdata_0, wt_rdata_1,

    // Systolic array weight input and psum
    output logic                           load_wt,
    output logic [COLS-1:0][WT_WIDTH-1:0]  wt_in,
    output logic [COLS-1:0][PSUM_WIDTH-1:0] psum_in,
    input  logic [COLS-1:0][PSUM_WIDTH-1:0] psum_out,

    // Stagger enable (to act_stagger module in top.sv)
    output logic stagger_en,

    // Accumulator SRAM interface
    output logic                       accum_we,
    output logic [M_ADDR_W-1:0]        accum_rd_addr,  // psum_in[0] lookup
    output logic [M_ADDR_W-1:0]        accum_wr_addr,  // output capture
    output logic [COLS*PSUM_WIDTH-1:0] accum_wdata,    // = psum_out (packed)
    input  logic [COLS*PSUM_WIDTH-1:0] accum_rdata     // old accum value for psum_in
);
    // ── State machine ─────────────────────────────────────────────────────────
    typedef enum logic [1:0] { IDLE, LOAD_WT, STREAM, DONE_ST } state_t;
    state_t state;

    logic [ADDR_WIDTH-1:0] wt_cnt;       // LOAD_WT row counter
    logic [8:0]            stream_cnt;  // STREAM cycle counter (needs 9 bits: max 256+32+1=289)
    logic [ADDR_WIDTH-1:0] ovlp_cnt;    // drain-overlap LOAD_WT row counter
    logic                  ovlp_active; // drain-overlap LOAD_WT in progress
    logic                  wt_preloaded_r; // next tile weights already shifted into PEs via drain-overlap

    // ── Weight SRAM mux ───────────────────────────────────────────────────────
    // During normal LOAD_WT read from buf_sel bank; during drain-overlap read
    // from the opposite bank (which holds the next tile's preloaded weights).
    logic [COLS*WT_WIDTH-1:0] wt_rdata_mux;
    assign wt_rdata_mux = (buf_sel ^ ovlp_active) ? wt_rdata_1 : wt_rdata_0;

    genvar c;
    generate
        for (c = 0; c < COLS; c++)
            assign wt_in[c] = wt_rdata_mux[c*WT_WIDTH +: WT_WIDTH];
    endgenerate

    // ── Weight SRAM addresses / read enables ─────────────────────────────────
    // During drain-overlap LOAD_WT use ovlp_cnt; otherwise use wt_cnt.
    assign wt_addr_0 = ovlp_active ? ovlp_cnt : wt_cnt;
    assign wt_addr_1 = ovlp_active ? ovlp_cnt : wt_cnt;

    logic in_load;
    assign in_load = (state == LOAD_WT);

    // Normal LOAD_WT reads from buf_sel bank; drain-overlap reads from ~buf_sel.
    assign wt_re_0 = (in_load && !buf_sel) || (ovlp_active &&  buf_sel);
    assign wt_re_1 = (in_load &&  buf_sel) || (ovlp_active && !buf_sel);

    assign load_wt    = in_load || ovlp_active;
    assign stagger_en = (state == STREAM);

    // ── STREAM timing ─────────────────────────────────────────────────────────
    // PE[ROWS-1] registers its output, so the result for M row m is available
    // in psum_out one cycle AFTER PE[ROWS-1] computes it.
    // PE[ROWS-1] computes M row m at posedge stream_cnt = m + ROWS.
    // Result is in psum_out at pre-posedge of stream_cnt = m + ROWS + 1.
    // → write at stream_cnt = m + ROWS + 1, so m_out = stream_cnt - ROWS - 1.
    logic [M_ADDR_W-1:0] m_out;
    logic out_valid;

    assign m_out     = stream_cnt[M_ADDR_W-1:0] - M_ADDR_W'(ROWS + 1);
    assign out_valid = (state == STREAM)
                    && (stream_cnt >= 9'(ROWS + 1))
                    && (stream_cnt <  9'(ROWS + 1) + 9'(M_count));

    // ── Accumulator SRAM drives ───────────────────────────────────────────────
    // rd_addr: M row PE[0] is currently processing = stream_cnt - 1
    // wr_addr: m_out (valid when out_valid)
    // wdata  : psum_out from PE[ROWS-1] (the FP32 addition already happened
    //          inside the systolic PEs — no separate adder needed here)
    assign accum_rd_addr = (stream_cnt > '0)
                           ? stream_cnt[M_ADDR_W-1:0] - M_ADDR_W'(1)
                           : '0;
    assign accum_wr_addr = m_out;
    assign accum_we      = out_valid;

    generate
        for (c = 0; c < COLS; c++)
            assign accum_wdata[c*PSUM_WIDTH +: PSUM_WIDTH] = psum_out[c];
    endgenerate

    // ── psum_in[0]: accumulated value or zero ─────────────────────────────────
    // PE[0] sees M row m = stream_cnt - 1 (1-cycle stagger delay for row 0).
    // For K-tile > 0: inject the previously accumulated partial sum so the
    // systolic PEs add each K-tile's contribution in place.
    logic use_accum;
    assign use_accum = (state == STREAM) && (stream_cnt != '0) && !first_k_tile;

    generate
        for (c = 0; c < COLS; c++)
            assign psum_in[c] = use_accum
                                 ? accum_rdata[c*PSUM_WIDTH +: PSUM_WIDTH]
                                 : '0;
    endgenerate

    // ── Done ─────────────────────────────────────────────────────────────────
    assign done = (state == DONE_ST);

    // ── State machine ─────────────────────────────────────────────────────────
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state          <= IDLE;
            wt_cnt         <= '0;
            stream_cnt     <= '0;
            ovlp_cnt       <= '0;
            ovlp_active    <= 1'b0;
            wt_preloaded_r <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    if (start) begin
                        if (wt_preloaded_r) begin
                            // Weights already shifted into PEs via drain-overlap;
                            // skip LOAD_WT and go directly to STREAM.
                            state          <= STREAM;
                            stream_cnt     <= '0;
                            wt_preloaded_r <= 1'b0;
                        end else begin
                            state  <= LOAD_WT;
                            wt_cnt <= '0;
                        end
                    end
                end

                LOAD_WT: begin
                    if (wt_cnt == ADDR_WIDTH'(ROWS - 1)) begin
                        state      <= STREAM;
                        stream_cnt <= '0;
                    end else
                        wt_cnt <= wt_cnt + 1'b1;
                end

                STREAM: begin
                    // M_count injection + ROWS fill/drain + 1 pipeline flush
                    if (stream_cnt == 9'(M_count) + 9'(ROWS) + 9'(1))
                        state <= DONE_ST;
                    else
                        stream_cnt <= stream_cnt + 1'b1;

                    // Drain-overlap LOAD_WT: begin loading next tile weights into
                    // PEs from the ~buf_sel SRAM bank at the start of the drain
                    // phase (stream_cnt == M_count+1).  Safety: PE[r]'s last
                    // non-zero activation arrives at stream_cnt = M_count+r;
                    // the new weight reaches PE[r] at M_count+1+r > M_count+r. ✓
                    if (!ovlp_active && preload_rdy &&
                            stream_cnt == 9'(M_count) + 9'(1)) begin
                        ovlp_active <= 1'b1;
                        ovlp_cnt    <= '0;
                    end
                    if (ovlp_active) begin
                        if (ovlp_cnt == ADDR_WIDTH'(ROWS - 1)) begin
                            ovlp_active    <= 1'b0;
                            wt_preloaded_r <= 1'b1;
                        end else
                            ovlp_cnt <= ovlp_cnt + 1'b1;
                    end
                end

                DONE_ST: state <= IDLE;
                default: state <= IDLE;
            endcase
        end
    end
endmodule
