`timescale 1ns / 1ps

// AXI4-Stream input interface for the 16×16 systolic accelerator.
//
// Routes incoming 256-bit beats based on the 3-bit tuser field.
//
// tuser encoding:
//   3'b000 = forward weight ping (fwd_wt_we_0)
//   3'b001 = forward weight pong (fwd_wt_we_1)
//   3'b010 = backward weight ping (bwd_wt_we_0)
//   3'b011 = backward weight pong (bwd_wt_we_1)
//   3'b100 = activation  (ROWS BF16 in lower 256 bits)
//   3'b110 = bias        (8 FP32 per beat, 2 beats for COLS=16)
//
// Weight writes: one 256-bit beat per weight row (COLS=16 × 16 bits = 256).
// Activation:    one beat per M row (ROWS=16 × 16 bits = 256).
// Bias:          two 256-bit beats (8 FP32 × 32 bits = 256 per beat).
//                beat_sel = wt_beat[0]:  0 → bias[0..7],  1 → bias[8..15].

module axi_sys #(
    parameter AXI_WIDTH  = 256,
    parameter TUSER_W    = 3,
    parameter COLS       = 16,
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

    // Forward weight SRAMs (ping / pong)
    output logic                        fwd_wt_we_0, fwd_wt_we_1,
    output logic [ADDR_WIDTH-1:0]       fwd_wt_addr,
    output logic [COLS*WT_WIDTH-1:0]    fwd_wt_data,

    // Backward (transposed) weight SRAMs (ping / pong)
    output logic                        bwd_wt_we_0, bwd_wt_we_1,
    output logic [ADDR_WIDTH-1:0]       bwd_wt_addr,
    output logic [COLS*WT_WIDTH-1:0]    bwd_wt_data,

    // Activation (streaming, one beat per M row)
    output logic                        act_we_0, act_we_1,
    output logic [ROWS*ACT_WIDTH-1:0]   act_data,

    // Bias SRAM write (two FP32 beats for COLS=16 values)
    output logic                              bias_we,
    output logic                              bias_beat_sel,  // 0=low half,1=high half
    output logic [(COLS/2)*32-1:0]            bias_data       // 8 FP32 per beat
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
    assign fwd_wt_data  = s_axis_tdata[COLS*WT_WIDTH-1:0];
    assign bwd_wt_data  = s_axis_tdata[COLS*WT_WIDTH-1:0];
    assign fwd_wt_addr  = wt_beat[ADDR_WIDTH-1:0];
    assign bwd_wt_addr  = wt_beat[ADDR_WIDTH-1:0];
    assign act_data     = s_axis_tdata[ROWS*ACT_WIDTH-1:0];
    assign bias_data    = s_axis_tdata[(COLS/2)*32-1:0];  // lower 256 bits = 8 FP32
    assign bias_beat_sel = wt_beat[0];                     // 0 for first beat, 1 for second

    // ── Write-enable decode ───────────────────────────────────────────────────
    logic is_weight, is_act, is_bias;
    assign is_bias   = (active_routing == 3'b110);
    assign is_act    =  active_routing[2] && !is_bias;
    assign is_weight = !active_routing[2] && !is_bias;

    assign fwd_wt_we_0 = handshake && is_weight && (active_routing[1:0] == 2'b00);
    assign fwd_wt_we_1 = handshake && is_weight && (active_routing[1:0] == 2'b01);
    assign bwd_wt_we_0 = handshake && is_weight && (active_routing[1:0] == 2'b10);
    assign bwd_wt_we_1 = handshake && is_weight && (active_routing[1:0] == 2'b11);
    assign act_we_0    = handshake && is_act    && !active_routing[0];
    assign act_we_1    = handshake && is_act    &&  active_routing[0];
    assign bias_we     = handshake && is_bias;

endmodule
