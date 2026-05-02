`timescale 1ns / 1ps

// Top-level: 16-row × 32-column BF16 weight-stationary systolic accelerator.
//
// Supports forward pass and backward dinp pass:
//   mode=0  Forward: out = act × W^T  (32 output channels per tile)
//   mode=1  Backward dinp: out = dout × W  (uses transposed weight SRAM)
//
// The host pre-loads the transposed weight matrix W^T into the backward SRAM
// via tuser=010/011. Hardware is identical for both passes; only the weight
// SRAM selected by mode differs.
//
// Reset: asynchronous active-low.
// FPU: custom BF16×BF16 multiplier (bf16_mul) + FP32 adder (bf16_fp32_add).
//
// AXI beat counts per tile (ROWS=16, COLS=32):
//   Weight write: 16 beats (1 per row, 32 BF16 = 512 bits each)
//   Act write:    1 beat   (16 BF16 in lower 256 bits)
//   Readback:     2 beats  (32 FP32 = 1024 bits in 2×512-bit beats)
//
// tuser encoding:
//   3'b000 = forward weight ping, 3'b001 = forward weight pong
//   3'b010 = backward weight ping, 3'b011 = backward weight pong
//   3'b100 = activation ping, 3'b101 = activation pong

module sys_top #(
    parameter AXI_WIDTH  = 512,
    parameter TUSER_W    = 3,
    parameter ROWS       = 16,
    parameter COLS       = 32,
    parameter ACT_WIDTH  = 16,
    parameter WT_WIDTH   = 16,
    parameter PSUM_WIDTH = 32,
    parameter ADDR_WIDTH = $clog2(ROWS)
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave (weights + activations)
    input  logic [AXI_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_W-1:0]   s_axis_tuser,
    input  logic                 s_axis_tvalid,
    output logic                 s_axis_tready,
    input  logic                 s_axis_tlast,

    // Host control
    input  logic start,
    input  logic mode,          // 0=forward, 1=backward dinp
    input  logic first_tile,
    input  logic last_tile,
    input  logic act_buf_sel,
    input  logic fwd_buf_sel,
    input  logic bwd_buf_sel,
    output logic done,

    // AXI4-Stream master (readback)
    input  logic                  rb_start,
    output logic                  rb_busy,
    output logic [AXI_WIDTH-1:0]  m_axis_tdata,
    output logic                  m_axis_tvalid,
    input  logic                  m_axis_tready,
    output logic                  m_axis_tlast
);
    // ── Internal signals ──────────────────────────────────────────────────────
    logic [ADDR_WIDTH-1:0]      fwd_wt_addr_0, fwd_wt_addr_1;
    logic [ADDR_WIDTH-1:0]      bwd_wt_addr_0, bwd_wt_addr_1;
    logic                       fwd_wt_re_0, fwd_wt_re_1;
    logic                       bwd_wt_re_0, bwd_wt_re_1;
    logic [COLS*WT_WIDTH-1:0]   fwd_wt_rdata_0, fwd_wt_rdata_1;
    logic [COLS*WT_WIDTH-1:0]   bwd_wt_rdata_0, bwd_wt_rdata_1;
    logic                       fwd_wt_we_0, fwd_wt_we_1;
    logic                       bwd_wt_we_0, bwd_wt_we_1;
    logic [ADDR_WIDTH-1:0]      axi_wt_addr;
    logic [COLS*WT_WIDTH-1:0]   axi_wt_data;
    logic                       act_we_0, act_we_1;
    logic                       act_re_0, act_re_1;
    logic [ROWS*ACT_WIDTH-1:0]  act_data, act_rdata_0, act_rdata_1;
    logic                       load_wt;
    logic [COLS-1:0][WT_WIDTH-1:0]   wt_in;
    logic [ROWS-1:0][ACT_WIDTH-1:0]  act_in;
    logic [COLS-1:0][PSUM_WIDTH-1:0] psum_in, psum_out;
    logic                       out_we;
    logic [COLS*PSUM_WIDTH-1:0] out_wdata, out_rdata;


    // ── AXI input buffer ──────────────────────────────────────────────────────
    axi_sys #(
        .AXI_WIDTH (AXI_WIDTH), .TUSER_W(TUSER_W),
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
        .act_we_0(act_we_0), .act_we_1(act_we_1), .act_data(act_data)
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

    // ── Weight SRAMs (backward/transposed ping/pong) ──────────────────────────
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

    // ── Activation SRAMs (ping/pong) ─────────────────────────────────────────
    act_sram #(.ROWS(ROWS), .ACT_WIDTH(ACT_WIDTH)) u_act_0 (
        .clk(clk), .we(act_we_0), .wdata(act_data),
        .rdata(act_rdata_0)
    );
    act_sram #(.ROWS(ROWS), .ACT_WIDTH(ACT_WIDTH)) u_act_1 (
        .clk(clk), .we(act_we_1), .wdata(act_data),
        .rdata(act_rdata_1)
    );

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
        .ACT_WIDTH(ACT_WIDTH), .WT_WIDTH(WT_WIDTH),
        .PSUM_WIDTH(PSUM_WIDTH), .ADDR_WIDTH(ADDR_WIDTH)
    ) u_ctrl (
        .clk(clk), .rst_n(rst_n),
        .start(start), .mode(mode), .first_tile(first_tile),
        .last_tile(last_tile), .act_buf_sel(act_buf_sel),
        .fwd_buf_sel(fwd_buf_sel), .bwd_buf_sel(bwd_buf_sel), .done(done),
        .fwd_wt_addr_0(fwd_wt_addr_0), .fwd_wt_addr_1(fwd_wt_addr_1),
        .fwd_wt_re_0(fwd_wt_re_0),     .fwd_wt_re_1(fwd_wt_re_1),
        .fwd_wt_rdata_0(fwd_wt_rdata_0), .fwd_wt_rdata_1(fwd_wt_rdata_1),
        .bwd_wt_addr_0(bwd_wt_addr_0), .bwd_wt_addr_1(bwd_wt_addr_1),
        .bwd_wt_re_0(bwd_wt_re_0),     .bwd_wt_re_1(bwd_wt_re_1),
        .bwd_wt_rdata_0(bwd_wt_rdata_0), .bwd_wt_rdata_1(bwd_wt_rdata_1),
        .act_re_0(act_re_0), .act_re_1(act_re_1),
        .act_rdata_0(act_rdata_0), .act_rdata_1(act_rdata_1),
        .load_wt(load_wt), .wt_in(wt_in), .act_in(act_in),
        .psum_in(psum_in), .psum_out(psum_out),
        .out_we(out_we), .out_wdata(out_wdata)
    );

    // ── Output SRAM ───────────────────────────────────────────────────────────
    output_sram #(.COLS(COLS), .PSUM_WIDTH(PSUM_WIDTH)) u_out (
        .clk(clk), .we(out_we), .wdata(out_wdata),
        .rdata(out_rdata)
    );

    // ── AXI readback ─────────────────────────────────────────────────────────
    axi_readback #(
        .COLS(COLS), .PSUM_WIDTH(PSUM_WIDTH), .AXI_WIDTH(AXI_WIDTH)
    ) u_rb (
        .clk(clk), .rst_n(rst_n),
        .start(rb_start), .busy(rb_busy),
        .sram_rdata(out_rdata),
        .m_axis_tdata(m_axis_tdata), .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready), .m_axis_tlast(m_axis_tlast)
    );

endmodule
