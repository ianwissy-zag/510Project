`timescale 1ns / 1ps

// Top-level: 32×32 BF16 weight-stationary systolic accelerator
//            with fused bias addition and GELU (Padé [3/2]) post-processing.
//
// Streaming dataflow (HDL_Systolic_32x32, with bias+GELU post-processing):
//   1. Load weight tile (ROWS beats, tuser=000/001).
//   2. Optionally load bias (2 beats, tuser=110).
//   3. Assert start with M_count.
//   4. Stream M_count activation beats (tuser=100).
//   5. Trigger rb_start with apply_bias / apply_gelu flags.
//      Readback streams 2×M_count processed beats.
//
// AXI tuser encoding:
//   3'b000/010 = weight ping  (bank 0, forward or backward)
//   3'b001/011 = weight pong  (bank 1, forward or backward)
//   3'b100     = activation (one beat per M row, full 512 bits used)
//   3'b110     = bias (2 beats: beat 0 → bias[0..15], beat 1 → bias[16..31])

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

    // AXI4-Stream slave (weights, activations, bias)
    input  logic [AXI_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_W-1:0]   s_axis_tuser,
    input  logic                 s_axis_tvalid,
    output logic                 s_axis_tready,
    input  logic                 s_axis_tlast,

    // Host control
    input  logic                start,
    input  logic                mode,
    input  logic                first_k_tile,
    input  logic                buf_sel,
    input  logic [M_ADDR_W-1:0] M_count,
    output logic                done,

    // Readback post-processing
    input  logic apply_bias,   // 1 → add bias[c] to each result column
    input  logic apply_gelu,   // 1 → apply GELU(Padé[3/2]) after bias

    // AXI4-Stream master (readback)
    input  logic                  rb_start,
    output logic                  rb_busy,
    output logic [AXI_WIDTH-1:0]  m_axis_tdata,
    output logic                  m_axis_tvalid,
    input  logic                  m_axis_tready,
    output logic                  m_axis_tlast
);
    // ── Internal ──────────────────────────────────────────────────────────────
    logic [ADDR_WIDTH-1:0]     wt_addr_0, wt_addr_1;
    logic                      wt_re_0, wt_re_1;
    logic [COLS*WT_WIDTH-1:0]  wt_rdata_0, wt_rdata_1;
    logic                      wt_we_0, wt_we_1;
    logic [ADDR_WIDTH-1:0]     axi_wt_addr;
    logic [COLS*WT_WIDTH-1:0]  axi_wt_data;

    logic                      act_we;
    logic [ROWS*ACT_WIDTH-1:0] act_data;
    logic [ROWS-1:0][ACT_WIDTH-1:0] stagger_in_packed, stagger_out_packed;

    logic                          stagger_en, load_wt;
    logic [COLS-1:0][WT_WIDTH-1:0]   wt_in;
    logic [ROWS-1:0][ACT_WIDTH-1:0]  act_in;
    logic [COLS-1:0][PSUM_WIDTH-1:0] psum_in, psum_out;

    logic                       accum_we;
    logic [M_ADDR_W-1:0]        accum_rd_addr, accum_wr_addr;
    logic [COLS*PSUM_WIDTH-1:0] accum_wdata, accum_rdata_a, accum_rdata_b;
    logic [M_ADDR_W-1:0]        rb_sram_addr;

    logic                           bias_we, bias_beat_sel;
    logic [(COLS/2)*PSUM_WIDTH-1:0] bias_data_axi;
    logic [COLS-1:0][PSUM_WIDTH-1:0] bias_rdata;

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
        .wt_we_0(wt_we_0), .wt_we_1(wt_we_1),
        .wt_addr(axi_wt_addr), .wt_data(axi_wt_data),
        .act_we_0(act_we), .act_we_1(), .act_data(act_data),
        .bias_we(bias_we), .bias_beat_sel(bias_beat_sel), .bias_data(bias_data_axi)
    );

    // ── Weight SRAMs (shared ping/pong for forward and backward) ─────────────
    weight_sram #(.COLS(COLS),.WT_WIDTH(WT_WIDTH),.DEPTH(ROWS)) u_wt_0 (
        .clk(clk), .we(wt_we_0),
        .addr(wt_we_0 ? axi_wt_addr : wt_addr_0),
        .wdata(axi_wt_data), .rdata(wt_rdata_0));
    weight_sram #(.COLS(COLS),.WT_WIDTH(WT_WIDTH),.DEPTH(ROWS)) u_wt_1 (
        .clk(clk), .we(wt_we_1),
        .addr(wt_we_1 ? axi_wt_addr : wt_addr_1),
        .wdata(axi_wt_data), .rdata(wt_rdata_1));

    // ── Bias SRAM ─────────────────────────────────────────────────────────────
    bias_sram #(.COLS(COLS), .DATA_WIDTH(PSUM_WIDTH)) u_bias (
        .clk(clk), .we(bias_we), .beat_sel(bias_beat_sel),
        .wdata(bias_data_axi), .rdata(bias_rdata));

    // ── Activation stagger ────────────────────────────────────────────────────
    genvar r;
    generate
        for (r = 0; r < ROWS; r++) begin : g_stagger_unpack
            assign stagger_in_packed[r] = act_we
                ? act_data[r*ACT_WIDTH +: ACT_WIDTH] : '0;
        end
    endgenerate

    act_stagger #(.ROWS(ROWS), .ACT_WIDTH(ACT_WIDTH)) u_stagger (
        .clk(clk), .rst_n(rst_n), .en(stagger_en),
        .act_in(stagger_in_packed), .act_out(stagger_out_packed));
    assign act_in = stagger_out_packed;

    // ── Systolic array ────────────────────────────────────────────────────────
    systolic_32x32 #(.ROWS(ROWS),.COLS(COLS),
                     .ACT_W(ACT_WIDTH),.WT_W(WT_WIDTH),.PSUM_W(PSUM_WIDTH))
    u_array (
        .clk(clk), .rst_n(rst_n), .load_wt(load_wt),
        .wt_in(wt_in), .act_in(act_in),
        .psum_in(psum_in), .psum_out(psum_out));

    // ── Controller ────────────────────────────────────────────────────────────
    controller #(.ROWS(ROWS),.COLS(COLS),.WT_WIDTH(WT_WIDTH),
                 .PSUM_WIDTH(PSUM_WIDTH),.M_MAX(M_MAX),
                 .ADDR_WIDTH(ADDR_WIDTH),.M_ADDR_W(M_ADDR_W))
    u_ctrl (
        .clk(clk), .rst_n(rst_n),
        .start(start), .mode(mode), .first_k_tile(first_k_tile),
        .buf_sel(buf_sel),
        .M_count(M_count), .done(done),
        .wt_addr_0(wt_addr_0), .wt_addr_1(wt_addr_1),
        .wt_re_0(wt_re_0), .wt_re_1(wt_re_1),
        .wt_rdata_0(wt_rdata_0), .wt_rdata_1(wt_rdata_1),
        .load_wt(load_wt), .wt_in(wt_in),
        .psum_in(psum_in), .psum_out(psum_out),
        .stagger_en(stagger_en),
        .accum_we(accum_we),
        .accum_rd_addr(accum_rd_addr), .accum_wr_addr(accum_wr_addr),
        .accum_wdata(accum_wdata), .accum_rdata(accum_rdata_a));

    // ── Accumulator SRAM ─────────────────────────────────────────────────────
    accum_sram #(.COLS(COLS),.PSUM_WIDTH(PSUM_WIDTH),
                 .M_MAX(M_MAX),.ADDR_WIDTH(M_ADDR_W))
    u_accum (
        .clk(clk), .we(accum_we),
        .wr_addr(accum_wr_addr), .wdata(accum_wdata),
        .rd_addr_a(accum_rd_addr), .rdata_a(accum_rdata_a),
        .rd_addr_b(rb_sram_addr),  .rdata_b(accum_rdata_b));

    // ── Readback with bias + GELU ─────────────────────────────────────────────
    axi_readback #(.COLS(COLS),.PSUM_WIDTH(PSUM_WIDTH),
                   .AXI_WIDTH(AXI_WIDTH),.M_MAX(M_MAX),.M_ADDR_W(M_ADDR_W))
    u_rb (
        .clk(clk), .rst_n(rst_n),
        .start(rb_start), .busy(rb_busy),
        .apply_bias(apply_bias), .apply_gelu(apply_gelu),
        .M_count(M_count),
        .sram_addr(rb_sram_addr), .sram_rdata(accum_rdata_b),
        .bias_rdata(bias_rdata),
        .m_axis_tdata(m_axis_tdata), .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready), .m_axis_tlast(m_axis_tlast));

endmodule
