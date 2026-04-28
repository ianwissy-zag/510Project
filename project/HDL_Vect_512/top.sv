`timescale 1ns / 1ps
// Top-level 128-wide vector MAC accelerator.
//
// Architecture contrast vs. systolic accelerator_top:
//   - No separate LOAD_WT phase. Weight rows stream from weight_sram
//     directly into 128 MAC units one row per cycle during COMPUTE.
//   - 128 parallel accumulators replace the 32×32 PE mesh.
//   - Each tile: K_DEPTH cycles of psum[j] += act[k] * W[k][j].
//   - Weight SRAM row width: 2048 bits (128 × 16-bit elements).
//   - Activation SRAM row width: 512 bits (32 × 16-bit elements packed).
//   - Output SRAM row width: 4096 bits (128 × 32-bit psums).
//   - AXI readback: 8 × 512-bit beats per output row.
//
// AXI-Stream tuser encoding:
//   2'b00=weight ping  2'b01=weight pong
//   2'b10=act ping     2'b11=act pong
//
// AXI beat counts (host responsibility):
//   Weight write: 4 beats per row × K_DEPTH rows = 128 beats
//   Act write:    1 beat per ping/pong slot (full K-vector packed)
//   Readback:     8 beats (4096-bit output in 512-bit chunks)
module vec_mac_top #(
    parameter AXI_DATA_WIDTH = 512,
    parameter TUSER_WIDTH    = 2,
    parameter VEC_SIZE       = 512,
    parameter ACT_WIDTH      = 16,
    parameter WT_WIDTH       = 16,
    parameter PSUM_WIDTH     = 32,
    parameter K_DEPTH        = 32,
    parameter K_ADDR_WIDTH   = $clog2(K_DEPTH)   // 5
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave  (host → accelerator: weights + activations)
    input  logic [AXI_DATA_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_WIDTH-1:0]    s_axis_tuser,
    input  logic                      s_axis_tvalid,
    output logic                      s_axis_tready,
    input  logic                      s_axis_tlast,

    // Compute control
    input  logic start,
    input  logic act_buf_sel,   // 0=ping, 1=pong
    input  logic first_tile,
    input  logic last_tile,
    input  logic wt_buf_sel,    // 0=read weight ping, 1=read weight pong
    output logic done,

    // AXI4-Stream master (accelerator → host: psum results)
    input  logic                      rb_start,
    output logic                      rb_busy,
    output logic [AXI_DATA_WIDTH-1:0] m_axis_tdata,
    output logic                      m_axis_tvalid,
    input  logic                      m_axis_tready,
    output logic                      m_axis_tlast
);

    localparam WT_DATA_WIDTH  = VEC_SIZE * WT_WIDTH;    // 2048
    localparam ACT_DATA_WIDTH = K_DEPTH  * ACT_WIDTH;   // 512
    localparam OUT_DATA_WIDTH = VEC_SIZE * PSUM_WIDTH;  // 4096
    localparam ACT_ADDR_WIDTH = 1;                       // depth=2 → 1-bit addr
    localparam OUT_ADDR_WIDTH = 1;                       // depth=2 → 1-bit addr

    // ── AXI buffer outputs ───────────────────────────────────────────────
    logic                       wt_we_0, wt_we_1;
    logic [K_ADDR_WIDTH-1:0]    axi_wt_addr;
    logic [WT_DATA_WIDTH-1:0]   axi_wt_data;
    logic                       act_we_0, act_we_1;
    logic [ACT_DATA_WIDTH-1:0]  axi_act_data;

    // ── Weight SRAM signals ──────────────────────────────────────────────
    logic [K_ADDR_WIDTH-1:0]    ctrl_wt_addr;
    logic [K_ADDR_WIDTH-1:0]    wt_sram_addr_0, wt_sram_addr_1;
    logic [WT_DATA_WIDTH-1:0]   wt_sram_rdata_0, wt_sram_rdata_1;
    logic [WT_DATA_WIDTH-1:0]   wt_sram_rdata;

    // ── Activation SRAM signals ──────────────────────────────────────────
    logic                       ctrl_act_re_0, ctrl_act_re_1;
    logic [0:0]                 ctrl_act_raddr;   // always 0
    logic [ACT_DATA_WIDTH-1:0]  act_rdata_0, act_rdata_1;

    // ── Output SRAM signals ───────────────────────────────────────────────
    logic                       ctrl_out_we;
    logic [OUT_ADDR_WIDTH-1:0]  ctrl_out_waddr;
    logic [OUT_DATA_WIDTH-1:0]  ctrl_out_wdata;
    logic                       rb_sram_re;
    logic [OUT_ADDR_WIDTH-1:0]  rb_sram_raddr;
    logic [OUT_DATA_WIDTH-1:0]  rb_sram_rdata;

    // ── MAC array signals ─────────────────────────────────────────────────
    logic                       ctrl_load_mac;
    logic                       ctrl_mac_en;
    logic [ACT_WIDTH-1:0]       ctrl_act_in;
    logic [WT_DATA_WIDTH-1:0]   ctrl_wt_in;
    logic [OUT_DATA_WIDTH-1:0]  ctrl_psum_seed;
    logic [OUT_DATA_WIDTH-1:0]  mac_psum_out;

    // ── Weight SRAM address mux (AXI write takes priority) ───────────────
    assign wt_sram_addr_0 = wt_we_0 ? axi_wt_addr : ctrl_wt_addr;
    assign wt_sram_addr_1 = wt_we_1 ? axi_wt_addr : ctrl_wt_addr;

    // ── Weight SRAM read-data mux (host selects bank before start) ────────
    assign wt_sram_rdata  = wt_buf_sel ? wt_sram_rdata_1 : wt_sram_rdata_0;

    // =========================================================
    // AXI input buffer
    // =========================================================
    axis_to_vect_buffer #(
        .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
        .TUSER_WIDTH   (TUSER_WIDTH),
        .VEC_SIZE      (VEC_SIZE),
        .WT_WIDTH      (WT_WIDTH),
        .K_DEPTH       (K_DEPTH),
        .K_ADDR_WIDTH  (K_ADDR_WIDTH)
    ) u_axi_buf (
        .clk           (clk),
        .rst_n         (rst_n),
        .s_axis_tdata  (s_axis_tdata),
        .s_axis_tuser  (s_axis_tuser),
        .s_axis_tvalid (s_axis_tvalid),
        .s_axis_tready (s_axis_tready),
        .s_axis_tlast  (s_axis_tlast),
        .wt_we_0       (wt_we_0),
        .wt_we_1       (wt_we_1),
        .wt_addr       (axi_wt_addr),
        .wt_data       (axi_wt_data),
        .act_we_0      (act_we_0),
        .act_we_1      (act_we_1),
        .act_data      (axi_act_data)
    );

    // =========================================================
    // Weight SRAMs — ping (bank 0) and pong (bank 1)
    // =========================================================
    weight_sram #(
        .DATA_WIDTH(WT_DATA_WIDTH),
        .DEPTH     (K_DEPTH),
        .ADDR_WIDTH(K_ADDR_WIDTH)
    ) u_wt_sram_0 (
        .clk   (clk),
        .we    (wt_we_0),
        .addr  (wt_sram_addr_0),
        .wdata (axi_wt_data),
        .rdata (wt_sram_rdata_0)
    );

    weight_sram #(
        .DATA_WIDTH(WT_DATA_WIDTH),
        .DEPTH     (K_DEPTH),
        .ADDR_WIDTH(K_ADDR_WIDTH)
    ) u_wt_sram_1 (
        .clk   (clk),
        .we    (wt_we_1),
        .addr  (wt_sram_addr_1),
        .wdata (axi_wt_data),
        .rdata (wt_sram_rdata_1)
    );

    // =========================================================
    // Activation SRAMs — ping (bank 0) and pong (bank 1)
    // =========================================================
    act_sram #(
        .DATA_WIDTH(ACT_DATA_WIDTH),
        .DEPTH     (2),
        .ADDR_WIDTH(ACT_ADDR_WIDTH)
    ) u_act_sram_0 (
        .clk   (clk),
        .we    (act_we_0),
        .waddr (1'b0),           // always row 0
        .wdata (axi_act_data),
        .re    (ctrl_act_re_0),
        .raddr (ctrl_act_raddr),
        .rdata (act_rdata_0)
    );

    act_sram #(
        .DATA_WIDTH(ACT_DATA_WIDTH),
        .DEPTH     (2),
        .ADDR_WIDTH(ACT_ADDR_WIDTH)
    ) u_act_sram_1 (
        .clk   (clk),
        .we    (act_we_1),
        .waddr (1'b0),
        .wdata (axi_act_data),
        .re    (ctrl_act_re_1),
        .raddr (ctrl_act_raddr),
        .rdata (act_rdata_1)
    );

    // =========================================================
    // Output SRAM
    // =========================================================
    output_sram #(
        .PSUM_WIDTH(PSUM_WIDTH),
        .VEC_SIZE  (VEC_SIZE),
        .DEPTH     (2),
        .ADDR_WIDTH(OUT_ADDR_WIDTH)
    ) u_out_sram (
        .clk   (clk),
        .we    (ctrl_out_we),
        .waddr (ctrl_out_waddr),
        .wdata (ctrl_out_wdata),
        .re    (rb_sram_re),
        .raddr (rb_sram_raddr),
        .rdata (rb_sram_rdata)
    );

    // =========================================================
    // Vector MAC array
    // =========================================================
    vec_mac_array #(
        .VEC_SIZE  (VEC_SIZE),
        .ACT_WIDTH (ACT_WIDTH),
        .WT_WIDTH  (WT_WIDTH),
        .PSUM_WIDTH(PSUM_WIDTH)
    ) u_mac (
        .clk      (clk),
        .rst_n    (rst_n),
        .load     (ctrl_load_mac),
        .mac_en   (ctrl_mac_en),
        .act_in   (ctrl_act_in),
        .wt_in    (ctrl_wt_in),
        .psum_seed(ctrl_psum_seed),
        .psum_out (mac_psum_out)
    );

    // =========================================================
    // Controller
    // =========================================================
    controller #(
        .VEC_SIZE      (VEC_SIZE),
        .ACT_WIDTH     (ACT_WIDTH),
        .WT_WIDTH      (WT_WIDTH),
        .PSUM_WIDTH    (PSUM_WIDTH),
        .K_DEPTH       (K_DEPTH),
        .K_ADDR_WIDTH  (K_ADDR_WIDTH),
        .OUT_ADDR_WIDTH(OUT_ADDR_WIDTH)
    ) u_ctrl (
        .clk         (clk),
        .rst_n       (rst_n),
        .start       (start),
        .act_buf_sel (act_buf_sel),
        .first_tile  (first_tile),
        .last_tile   (last_tile),
        .done        (done),
        .wt_addr     (ctrl_wt_addr),
        .wt_rdata    (wt_sram_rdata),
        .act_re_0    (ctrl_act_re_0),
        .act_re_1    (ctrl_act_re_1),
        .act_raddr   (ctrl_act_raddr),
        .act_rdata_0 (act_rdata_0),
        .act_rdata_1 (act_rdata_1),
        .load_mac    (ctrl_load_mac),
        .mac_en      (ctrl_mac_en),
        .act_in      (ctrl_act_in),
        .wt_in       (ctrl_wt_in),
        .psum_seed   (ctrl_psum_seed),
        .psum_out    (mac_psum_out),
        .out_we      (ctrl_out_we),
        .out_waddr   (ctrl_out_waddr),
        .out_wdata   (ctrl_out_wdata)
    );

    // =========================================================
    // AXI readback
    // =========================================================
    axi_readback #(
        .PSUM_WIDTH    (PSUM_WIDTH),
        .VEC_SIZE      (VEC_SIZE),
        .N_ROWS        (1),
        .DEPTH         (2),
        .ADDR_WIDTH    (OUT_ADDR_WIDTH)
    ) u_rb (
        .clk          (clk),
        .rst_n        (rst_n),
        .start        (rb_start),
        .busy         (rb_busy),
        .sram_re      (rb_sram_re),
        .sram_raddr   (rb_sram_raddr),
        .sram_rdata   (rb_sram_rdata),
        .m_axis_tdata (m_axis_tdata),
        .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tready(m_axis_tready),
        .m_axis_tlast (m_axis_tlast)
    );

endmodule
