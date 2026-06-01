`timescale 1ns / 1ps

// AXI4-Stream input interface for the 32×32 systolic accelerator.
//
// Routes incoming 512-bit beats based on the 3-bit tuser field.
//
// tuser encoding:
//   3'b000 = weight ping  (wt_we_0) — forward or backward, bank 0
//   3'b001 = weight pong  (wt_we_1) — forward or backward, bank 1
//   3'b010 = weight ping  (wt_we_0) — alias (backward tuser, same bank)
//   3'b011 = weight pong  (wt_we_1) — alias (backward tuser, same bank)
//   3'b100 = activation  (ROWS BF16 in lower 512 bits)
//   3'b110 = bias        (16 FP32 per beat, 2 beats for COLS=32)
//
// Weight writes: one 512-bit beat per weight row (COLS=32 × 16 bits = 512).
// Activation:    one beat per M row (ROWS=32 × 16 bits = 512).
// Bias:          two 512-bit beats (16 FP32 × 32 bits = 512 per beat).
//                beat_sel = wt_beat[0]:  0 → bias[0..15],  1 → bias[16..31].

module axi_sys #(
    parameter AXI_WIDTH  = 512,
    parameter TUSER_W    = 3,
    parameter COLS       = 32,
    parameter ROWS       = 16,
    parameter ACT_WIDTH  = 16,
    parameter WT_WIDTH   = 16,
    parameter ADDR_WIDTH = $clog2(ROWS)
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave
    input  logic [AXI_WIDTH-1:0]  s_axis_tdata,
    input  logic [TUSER_W-1:0]    s_axis_tuser,
    input  logic                  s_axis_tvalid,
    output logic                  s_axis_tready,
    input  logic                  s_axis_tlast,

    // Weight SRAMs — shared ping/pong for forward and backward
    output logic                        wt_we_0, wt_we_1,
    output logic [ADDR_WIDTH-1:0]       wt_addr,
    output logic [COLS*WT_WIDTH-1:0]    wt_data,

    // Activation (streaming, one beat per M row)
    output logic                        act_we_0, act_we_1,
    output logic [ROWS*ACT_WIDTH-1:0]   act_data,

    // Bias SRAM write (two FP32 beats for COLS=32 values)
    output logic                              bias_we,
    output logic                              bias_beat_sel,  // 0=low half,1=high half
    output logic [(COLS/2)*32-1:0]            bias_data       // 16 FP32 per beat
);
    logic [TUSER_W-1:0]  routing;
    logic [ADDR_WIDTH:0] wt_beat;
    logic                handshake;

    assign handshake     = s_axis_tvalid && s_axis_tready;
    assign s_axis_tready = rst_n;

    logic [TUSER_W-1:0] active_routing;
    assign active_routing = (wt_beat == '0) ? s_axis_tuser : routing;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) routing <= '0;
        else if (handshake && wt_beat == '0) routing <= s_axis_tuser;
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) wt_beat <= '0;
        else if (handshake)
            wt_beat <= s_axis_tlast ? '0 : wt_beat + 1'b1;
    end

    // ── Data extracts ─────────────────────────────────────────────────────────
    assign wt_data       = s_axis_tdata[COLS*WT_WIDTH-1:0];
    assign wt_addr       = wt_beat[ADDR_WIDTH-1:0];
    assign act_data      = s_axis_tdata[ROWS*ACT_WIDTH-1:0];
    assign bias_data     = s_axis_tdata[(COLS/2)*32-1:0];
    assign bias_beat_sel = wt_beat[0];

    // ── Write-enable decode ───────────────────────────────────────────────────
    // tuser bit[2]=0 → weight (any bank), bit[2]=1 → act or bias.
    // tuser bit[0] selects bank 0 vs 1; bit[1] (fwd/bwd distinction) is ignored.
    logic is_weight, is_act, is_bias;
    assign is_bias   = (active_routing == 3'b110);
    assign is_act    =  active_routing[2] && !is_bias;
    assign is_weight = !active_routing[2];

    assign wt_we_0  = handshake && is_weight && !active_routing[0];
    assign wt_we_1  = handshake && is_weight &&  active_routing[0];
    assign act_we_0 = handshake && is_act    && !active_routing[0];
    assign act_we_1 = handshake && is_act    &&  active_routing[0];
    assign bias_we  = handshake && is_bias;

endmodule
