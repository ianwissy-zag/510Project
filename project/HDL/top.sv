`timescale 1ns / 1ps
/* verilator lint_off DECLFILENAME */
// Top-level accelerator chip for GPT-2 small matrix-multiply offload.
//
// Data flow (one tile computation):
//   1. Host streams weights over AXI-S (tuser=2'b00 / 2'b01).
//      axis_to_pingpong_buffer writes them to weight_sram (ping or pong).
//   2. Host streams activations over AXI-S (tuser=2'b10 / 2'b11).
//      axis_to_pingpong_buffer writes them to act_sram_0 or act_sram_1.
//   3. Host asserts start + act_buf_sel.  controller loads weights from
//      weight_sram into the systolic array (LOAD_WT, 64 cycles), then
//      feeds the activation vector (COMPUTE, 64 cycles), then writes
//      psum_out to output_sram row 0 (CAPTURE).  done pulses for one cycle.
//   4. Host asserts rb_start.  axi_readback streams output_sram rows back
//      over the master AXI-S port (2 × 512-bit beats per row, 32 rows total).
//
// Weight SRAM address arbitration:
//   AXI write (wt_we_0 active) takes priority; otherwise controller drives
//   the address during LOAD_WT.  Phases are non-overlapping by design.
//
// Simulation overrides (see Makefile):
//   -GWT_DEPTH=64  -GWT_ADDR_WIDTH=6   (hardware defaults: 81920 / 17)
module accelerator_top #(
    parameter AXI_DATA_WIDTH = 512,
    parameter TUSER_WIDTH    = 2,
    parameter ARRAY_SIZE     = 32,
    parameter ACT_WIDTH      = 16,
    parameter WT_WIDTH       = 16,
    parameter PSUM_WIDTH     = 32,
    parameter WT_DEPTH       = ARRAY_SIZE,
    parameter WT_ADDR_WIDTH  = $clog2(WT_DEPTH),
    parameter ACT_DEPTH      = 32,
    parameter OUT_DEPTH      = 32
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
    input  logic act_buf_sel,
    input  logic first_tile,
    input  logic last_tile,
    input  logic wt_buf_sel,   // 0 = read weight ping, 1 = read weight pong
    output logic done,

    // AXI4-Stream master (accelerator → host: psum results)
    input  logic                      rb_start,
    output logic                      rb_busy,
    output logic [AXI_DATA_WIDTH-1:0] m_axis_tdata,
    output logic                      m_axis_tvalid,
    input  logic                      m_axis_tready,
    output logic                      m_axis_tlast
);

    localparam ACT_ADDR_WIDTH = $clog2(ACT_DEPTH);  // 5
    localparam OUT_ADDR_WIDTH = $clog2(OUT_DEPTH);   // 5
    localparam DATA_WIDTH     = PSUM_WIDTH * ARRAY_SIZE; // 1024

    // ── Internal signal declarations ─────────────────────────────────────
    // AXI buffer outputs
    logic                      wt_we_0;
    logic                      wt_we_1;
    logic [ACT_ADDR_WIDTH-1:0] axi_wt_addr_5b;   // 5-bit from buffer
    logic [AXI_DATA_WIDTH-1:0] axi_wt_data;
    logic                      act_we_0, act_we_1;
    logic [ACT_ADDR_WIDTH-1:0] axi_act_addr;
    logic [AXI_DATA_WIDTH-1:0] axi_act_data;

    // Weight SRAMs (ping/pong)
    logic [WT_ADDR_WIDTH-1:0]  axi_wt_addr;  // zero-extended
    logic [WT_ADDR_WIDTH-1:0]  ctrl_wt_addr;
    logic [WT_ADDR_WIDTH-1:0]  wt_sram_addr_0, wt_sram_addr_1;
    logic [AXI_DATA_WIDTH-1:0] wt_sram_rdata_0, wt_sram_rdata_1;
    logic [AXI_DATA_WIDTH-1:0] wt_sram_rdata;

    // Activation SRAMs
    logic                      ctrl_act_re_0, ctrl_act_re_1;
    logic [ACT_ADDR_WIDTH-1:0] ctrl_act_raddr;
    logic [AXI_DATA_WIDTH-1:0] act_rdata_0, act_rdata_1;

    // Output SRAM – write (controller) and read (readback)
    logic                      ctrl_out_we;
    logic [OUT_ADDR_WIDTH-1:0] ctrl_out_waddr;
    logic [DATA_WIDTH-1:0]     ctrl_out_wdata;
    logic                      rb_sram_re;
    logic [OUT_ADDR_WIDTH-1:0] rb_sram_raddr;
    logic [DATA_WIDTH-1:0]     rb_sram_rdata;

    // Systolic array
    logic                                   ctrl_load_wt;
    logic [ARRAY_SIZE-1:0][ACT_WIDTH-1:0]  ctrl_act_in;
    logic [ARRAY_SIZE-1:0][WT_WIDTH-1:0]   ctrl_wt_in;
    logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] ctrl_psum_in;
    logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] sys_psum_out;

    // ── AXI address zero-extension ────────────────────────────────────────
    assign axi_wt_addr  = WT_ADDR_WIDTH'(axi_wt_addr_5b);

    // ── Weight SRAM address muxes (AXI write takes priority) ─────────────
    assign wt_sram_addr_0 = wt_we_0 ? axi_wt_addr : ctrl_wt_addr;
    assign wt_sram_addr_1 = wt_we_1 ? axi_wt_addr : ctrl_wt_addr;

    // ── Weight SRAM read-data mux (host selects bank before asserting start)
    assign wt_sram_rdata  = wt_buf_sel ? wt_sram_rdata_1 : wt_sram_rdata_0;

    // =========================================================
    // AXI input buffer
    // =========================================================
    axis_to_pingpong_buffer #(
        .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
        .TUSER_WIDTH   (TUSER_WIDTH),
        .SRAM_DEPTH    (ACT_DEPTH),
        .ADDR_WIDTH    (ACT_ADDR_WIDTH)
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
        .wt_addr       (axi_wt_addr_5b),
        .wt_data       (axi_wt_data),
        .act_we_0      (act_we_0),
        .act_we_1      (act_we_1),
        .act_addr      (axi_act_addr),
        .act_data      (axi_act_data)
    );

    // =========================================================
    // Weight SRAMs — ping (bank 0) and pong (bank 1)
    // =========================================================
    weight_sram #(
        .DATA_WIDTH(AXI_DATA_WIDTH),
        .DEPTH     (WT_DEPTH),
        .ADDR_WIDTH(WT_ADDR_WIDTH)
    ) u_wt_sram_0 (
        .clk   (clk),
        .we    (wt_we_0),
        .addr  (wt_sram_addr_0),
        .wdata (axi_wt_data),
        .rdata (wt_sram_rdata_0)
    );

    weight_sram #(
        .DATA_WIDTH(AXI_DATA_WIDTH),
        .DEPTH     (WT_DEPTH),
        .ADDR_WIDTH(WT_ADDR_WIDTH)
    ) u_wt_sram_1 (
        .clk   (clk),
        .we    (wt_we_1),
        .addr  (wt_sram_addr_1),
        .wdata (axi_wt_data),
        .rdata (wt_sram_rdata_1)
    );

    // =========================================================
    // Activation SRAMs (ping and pong)
    // =========================================================
    act_sram #(
        .DATA_WIDTH(AXI_DATA_WIDTH),
        .DEPTH     (ACT_DEPTH),
        .ADDR_WIDTH(ACT_ADDR_WIDTH)
    ) u_act_sram_0 (
        .clk   (clk),
        .we    (act_we_0),
        .waddr (axi_act_addr),
        .wdata (axi_act_data),
        .re    (ctrl_act_re_0),
        .raddr (ctrl_act_raddr),
        .rdata (act_rdata_0)
    );

    act_sram #(
        .DATA_WIDTH(AXI_DATA_WIDTH),
        .DEPTH     (ACT_DEPTH),
        .ADDR_WIDTH(ACT_ADDR_WIDTH)
    ) u_act_sram_1 (
        .clk   (clk),
        .we    (act_we_1),
        .waddr (axi_act_addr),
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
        .ARRAY_SIZE(ARRAY_SIZE),
        .DEPTH     (OUT_DEPTH),
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
    // Systolic array
    // =========================================================
    systolic_array_32x32 u_systolic (
        .clk     (clk),
        .rst_n   (rst_n),
        .load_wt (ctrl_load_wt),
        .act_in  (ctrl_act_in),
        .wt_in   (ctrl_wt_in),
        .psum_in (ctrl_psum_in),
        .psum_out(sys_psum_out)
    );

    // =========================================================
    // Controller
    // =========================================================
    controller #(
        .ARRAY_SIZE    (ARRAY_SIZE),
        .ACT_WIDTH     (ACT_WIDTH),
        .WT_WIDTH      (WT_WIDTH),
        .PSUM_WIDTH    (PSUM_WIDTH),
        .WT_ADDR_WIDTH (WT_ADDR_WIDTH),
        .ACT_ADDR_WIDTH(ACT_ADDR_WIDTH),
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
        .load_wt     (ctrl_load_wt),
        .act_in      (ctrl_act_in),
        .wt_in       (ctrl_wt_in),
        .psum_in     (ctrl_psum_in),
        .psum_out    (sys_psum_out),
        .out_we      (ctrl_out_we),
        .out_waddr   (ctrl_out_waddr),
        .out_wdata   (ctrl_out_wdata)
    );

    // =========================================================
    // AXI readback
    // =========================================================
    axi_readback #(
        .PSUM_WIDTH(PSUM_WIDTH),
        .ARRAY_SIZE(ARRAY_SIZE),
        .DEPTH     (OUT_DEPTH),
        .ADDR_WIDTH(OUT_ADDR_WIDTH)
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
