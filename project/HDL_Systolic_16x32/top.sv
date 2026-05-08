`timescale 1ns / 1ps

// Top-level: 16×32 BF16 weight-stationary systolic accelerator — streaming mode.
//
// Streaming dataflow (replaces the old tile-by-tile COMPUTE/CAPTURE approach):
//   1. Host loads one K-tile of weights via AXI (16 beats, tuser=000/001).
//   2. Host asserts start with M_count = number of M rows to process.
//   3. Controller runs LOAD_WT (16 cycles) then STREAM (M_count+ROWS cycles).
//   4. During STREAM the host streams M_count activation beats (tuser=100/101),
//      one per cycle.  The act_stagger module staggers each row's activation
//      by r+1 cycles so all ROWS PE rows process different M rows simultaneously.
//   5. PE[ROWS-1] produces one valid output row per cycle after the ROWS-cycle
//      fill.  Results accumulate in accum_sram across K-tiles.
//   6. After the last K-tile, host triggers rb_start; M_count×2 AXI beats
//      stream the complete M×N result block.
//
// K-tile accumulation: psum_in[0] is seeded from accum_sram on K-tiles > 0.
//   The systolic PEs perform the FP32 addition; no separate adder is needed.
//
// AXI tuser encoding (unchanged):
//   3'b000 = forward weight ping,  3'b001 = forward weight pong
//   3'b010 = backward weight ping, 3'b011 = backward weight pong
//   3'b100 = activation (streaming, one beat per M row)

module sys_top #(
    parameter AXI_WIDTH  = 512,
    parameter TUSER_W    = 3,
    parameter ROWS       = 32,
    parameter COLS       = 32,
    parameter ACT_WIDTH  = 16,
    parameter WT_WIDTH   = 16,
    parameter PSUM_WIDTH = 32,
    parameter M_MAX      = 256,
    parameter ADDR_WIDTH = $clog2(ROWS),
    parameter M_ADDR_W   = $clog2(M_MAX + 1)
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave (weights + streaming activations)
    input  logic [AXI_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_W-1:0]   s_axis_tuser,
    input  logic                 s_axis_tvalid,
    output logic                 s_axis_tready,
    input  logic                 s_axis_tlast,

    // Host control
    input  logic                start,
    input  logic                mode,         // 0=forward, 1=backward
    input  logic                first_k_tile, // 1→reset accumulator
    input  logic                fwd_buf_sel,
    input  logic                bwd_buf_sel,
    input  logic [M_ADDR_W-1:0] M_count,      // M rows to stream
    output logic                done,

    // AXI4-Stream master (readback)
    input  logic                  rb_start,
    output logic                  rb_busy,
    output logic [AXI_WIDTH-1:0]  m_axis_tdata,
    output logic                  m_axis_tvalid,
    input  logic                  m_axis_tready,
    output logic                  m_axis_tlast
);
    // ── Internal signals ──────────────────────────────────────────────────────
    logic [ADDR_WIDTH-1:0]     fwd_wt_addr_0, fwd_wt_addr_1;
    logic [ADDR_WIDTH-1:0]     bwd_wt_addr_0, bwd_wt_addr_1;
    logic                      fwd_wt_re_0,   fwd_wt_re_1;
    logic                      bwd_wt_re_0,   bwd_wt_re_1;
    logic [COLS*WT_WIDTH-1:0]  fwd_wt_rdata_0, fwd_wt_rdata_1;
    logic [COLS*WT_WIDTH-1:0]  bwd_wt_rdata_0, bwd_wt_rdata_1;
    logic                      fwd_wt_we_0, fwd_wt_we_1;
    logic                      bwd_wt_we_0, bwd_wt_we_1;
    logic [ADDR_WIDTH-1:0]     axi_wt_addr;
    logic [COLS*WT_WIDTH-1:0]  axi_wt_data;

    // Activation from AXI — routed directly to stagger (no act_sram)
    logic                      act_we;          // any activation beat valid
    logic [ROWS*ACT_WIDTH-1:0] act_data;        // from AXI bus
    logic [ROWS*ACT_WIDTH-1:0] stagger_in;      // gated: 0 during drain
    logic [ROWS-1:0][ACT_WIDTH-1:0] stagger_in_packed;
    logic [ROWS-1:0][ACT_WIDTH-1:0] stagger_out_packed;

    logic                          stagger_en;
    logic                          load_wt;
    logic [COLS-1:0][WT_WIDTH-1:0]   wt_in;
    logic [ROWS-1:0][ACT_WIDTH-1:0]  act_in;
    logic [COLS-1:0][PSUM_WIDTH-1:0] psum_in, psum_out;

    // Accumulator SRAM
    logic                       accum_we;
    logic [M_ADDR_W-1:0]        accum_rd_addr, accum_wr_addr;
    logic [COLS*PSUM_WIDTH-1:0] accum_wdata, accum_rdata_a, accum_rdata_b;
    logic [M_ADDR_W-1:0]        rb_sram_addr;

    // ── AXI input ─────────────────────────────────────────────────────────────
    axi_sys #(
        .AXI_WIDTH(AXI_WIDTH), .TUSER_W(TUSER_W),
        .COLS(COLS), .ROWS(ROWS),
        .ACT_WIDTH(ACT_WIDTH), .WT_WIDTH(WT_WIDTH), .ADDR_WIDTH(ADDR_WIDTH)
    ) u_axi (
        .clk(clk), .rst_n(rst_n),
        .s_axis_tdata(s_axis_tdata), .s_axis_tuser(s_axis_tuser),
        .s_axis_tvalid(s_axis_tvalid), .s_axis_tready(s_axis_tready),
        .s_axis_tlast(s_axis_tlast),
        .fwd_wt_we_0(fwd_wt_we_0), .fwd_wt_we_1(fwd_wt_we_1),
        .fwd_wt_addr(axi_wt_addr),  .fwd_wt_data(axi_wt_data),
        .bwd_wt_we_0(bwd_wt_we_0), .bwd_wt_we_1(bwd_wt_we_1),
        .bwd_wt_addr(),             .bwd_wt_data(),
        .act_we_0(act_we), .act_we_1(), .act_data(act_data)
    );

    // ── Weight SRAMs (forward ping/pong) ─────────────────────────────────────
    weight_sram #(.COLS(COLS), .WT_WIDTH(WT_WIDTH), .DEPTH(ROWS)) u_fwd_wt_0 (
        .clk(clk),
        .we(fwd_wt_we_0), .addr(fwd_wt_we_0 ? axi_wt_addr : fwd_wt_addr_0),
        .wdata(axi_wt_data), .rdata(fwd_wt_rdata_0)
    );
    weight_sram #(.COLS(COLS), .WT_WIDTH(WT_WIDTH), .DEPTH(ROWS)) u_fwd_wt_1 (
        .clk(clk),
        .we(fwd_wt_we_1), .addr(fwd_wt_we_1 ? axi_wt_addr : fwd_wt_addr_1),
        .wdata(axi_wt_data), .rdata(fwd_wt_rdata_1)
    );
    weight_sram #(.COLS(COLS), .WT_WIDTH(WT_WIDTH), .DEPTH(ROWS)) u_bwd_wt_0 (
        .clk(clk),
        .we(bwd_wt_we_0), .addr(bwd_wt_we_0 ? axi_wt_addr : bwd_wt_addr_0),
        .wdata(axi_wt_data), .rdata(bwd_wt_rdata_0)
    );
    weight_sram #(.COLS(COLS), .WT_WIDTH(WT_WIDTH), .DEPTH(ROWS)) u_bwd_wt_1 (
        .clk(clk),
        .we(bwd_wt_we_1), .addr(bwd_wt_we_1 ? axi_wt_addr : bwd_wt_addr_1),
        .wdata(axi_wt_data), .rdata(bwd_wt_rdata_1)
    );

    // ── Activation stagger ────────────────────────────────────────────────────
    // Gate act_data to 0 when no AXI activation beat is present (drain cycles).
    // This lets the stagger pipeline drain correctly without injecting stale data.
    assign stagger_in = act_we ? act_data : '0;

    genvar r;
    generate
        for (r = 0; r < ROWS; r++) begin : g_stagger_unpack
            assign stagger_in_packed[r] = stagger_in[r*ACT_WIDTH +: ACT_WIDTH];
        end
    endgenerate

    act_stagger #(.ROWS(ROWS), .ACT_WIDTH(ACT_WIDTH)) u_stagger (
        .clk(clk), .rst_n(rst_n),
        .en(stagger_en),
        .act_in(stagger_in_packed),
        .act_out(stagger_out_packed)
    );

    assign act_in = stagger_out_packed;

    // ── Systolic array ────────────────────────────────────────────────────────
    systolic_16x32 #(
        .ROWS(ROWS), .COLS(COLS),
        .ACT_W(ACT_WIDTH), .WT_W(WT_WIDTH), .PSUM_W(PSUM_WIDTH)
    ) u_array (
        .clk(clk), .rst_n(rst_n), .load_wt(load_wt),
        .wt_in(wt_in), .act_in(act_in),
        .psum_in(psum_in), .psum_out(psum_out)
    );

    // ── Controller ────────────────────────────────────────────────────────────
    controller #(
        .ROWS(ROWS), .COLS(COLS),
        .WT_WIDTH(WT_WIDTH), .PSUM_WIDTH(PSUM_WIDTH),
        .M_MAX(M_MAX), .ADDR_WIDTH(ADDR_WIDTH), .M_ADDR_W(M_ADDR_W)
    ) u_ctrl (
        .clk(clk), .rst_n(rst_n),
        .start(start), .mode(mode),
        .first_k_tile(first_k_tile),
        .fwd_buf_sel(fwd_buf_sel), .bwd_buf_sel(bwd_buf_sel),
        .M_count(M_count), .done(done),
        .fwd_wt_addr_0(fwd_wt_addr_0), .fwd_wt_addr_1(fwd_wt_addr_1),
        .fwd_wt_re_0(fwd_wt_re_0),     .fwd_wt_re_1(fwd_wt_re_1),
        .fwd_wt_rdata_0(fwd_wt_rdata_0), .fwd_wt_rdata_1(fwd_wt_rdata_1),
        .bwd_wt_addr_0(bwd_wt_addr_0), .bwd_wt_addr_1(bwd_wt_addr_1),
        .bwd_wt_re_0(bwd_wt_re_0),     .bwd_wt_re_1(bwd_wt_re_1),
        .bwd_wt_rdata_0(bwd_wt_rdata_0), .bwd_wt_rdata_1(bwd_wt_rdata_1),
        .load_wt(load_wt), .wt_in(wt_in),
        .psum_in(psum_in), .psum_out(psum_out),
        .stagger_en(stagger_en),
        .accum_we(accum_we),
        .accum_rd_addr(accum_rd_addr), .accum_wr_addr(accum_wr_addr),
        .accum_wdata(accum_wdata),     .accum_rdata(accum_rdata_a)
    );

    // ── Accumulator SRAM ─────────────────────────────────────────────────────
    accum_sram #(
        .COLS(COLS), .PSUM_WIDTH(PSUM_WIDTH),
        .M_MAX(M_MAX), .ADDR_WIDTH(M_ADDR_W)
    ) u_accum (
        .clk(clk),
        .we(accum_we),
        .wr_addr(accum_wr_addr),
        .wdata(accum_wdata),
        .rd_addr_a(accum_rd_addr),
        .rdata_a(accum_rdata_a),
        .rd_addr_b(rb_sram_addr),
        .rdata_b(accum_rdata_b)
    );

    // ── AXI readback ─────────────────────────────────────────────────────────
    axi_readback #(
        .COLS(COLS), .PSUM_WIDTH(PSUM_WIDTH),
        .AXI_WIDTH(AXI_WIDTH),
        .M_MAX(M_MAX), .M_ADDR_W(M_ADDR_W)
    ) u_rb (
        .clk(clk), .rst_n(rst_n),
        .start(rb_start), .busy(rb_busy),
        .M_count(M_count),
        .sram_addr(rb_sram_addr),
        .sram_rdata(accum_rdata_b),
        .m_axis_tdata(m_axis_tdata), .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready), .m_axis_tlast(m_axis_tlast)
    );

endmodule
