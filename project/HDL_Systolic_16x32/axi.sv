`timescale 1ns / 1ps

// AXI4-Stream input interface for the 16×32 systolic accelerator.
//
// Routes incoming 512-bit beats to forward weight SRAMs, backward (transposed)
// weight SRAMs, or activation SRAMs based on the 3-bit tuser field.
//
// Reset: asynchronous active-low. tready deasserts during reset.
//
// tuser encoding (latched from beat 0, held for packet):
//   3'b000 = forward weight ping (fwd_wt_we_0)
//   3'b001 = forward weight pong (fwd_wt_we_1)
//   3'b010 = backward weight ping (bwd_wt_we_0)
//   3'b011 = backward weight pong (bwd_wt_we_1)
//   3'b100 = activation ping (act_we_0)
//   3'b101 = activation pong (act_we_1)
//
// Weight writes: one 512-bit beat per weight row (COLS=32 × 16-bit = 512 bits).
//   ROWS beats complete one weight tile. addr increments each beat.
//
// Activation writes: one 512-bit beat per ping/pong slot.
//   Lower ROWS×16 bits used; upper bits ignored.

module axi_sys #(
    parameter AXI_WIDTH  = 512,
    parameter TUSER_W    = 3,
    parameter COLS       = 32,   // weight columns
    parameter ROWS       = 16,   // weight rows = K_DEPTH
    parameter ACT_WIDTH  = 16,   // BF16
    parameter WT_WIDTH   = 16,   // BF16
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

    // Activation SRAMs (ping / pong)
    output logic                        act_we_0, act_we_1,
    output logic [ROWS*ACT_WIDTH-1:0]   act_data
);
    logic [TUSER_W-1:0]    routing;
    logic [ADDR_WIDTH:0]   wt_beat;   // beat counter within weight packet
    logic                  handshake;

    assign handshake    = s_axis_tvalid && s_axis_tready;
    assign s_axis_tready = rst_n;

    // Latch routing from first beat
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

    // Weight data: lower COLS×WT_WIDTH bits of AXI beat
    assign fwd_wt_data = s_axis_tdata[COLS*WT_WIDTH-1:0];
    assign bwd_wt_data = s_axis_tdata[COLS*WT_WIDTH-1:0];
    assign fwd_wt_addr = wt_beat[ADDR_WIDTH-1:0];
    assign bwd_wt_addr = wt_beat[ADDR_WIDTH-1:0];

    // Activation data: lower ROWS×ACT_WIDTH bits
    assign act_data = s_axis_tdata[ROWS*ACT_WIDTH-1:0];

    // Write enable decode
    logic is_act;
    assign is_act = active_routing[2];   // tuser[2]=1 → activation

    assign fwd_wt_we_0 = handshake && !is_act && (active_routing[1:0] == 2'b00);
    assign fwd_wt_we_1 = handshake && !is_act && (active_routing[1:0] == 2'b01);
    assign bwd_wt_we_0 = handshake && !is_act && (active_routing[1:0] == 2'b10);
    assign bwd_wt_we_1 = handshake && !is_act && (active_routing[1:0] == 2'b11);
    assign act_we_0    = handshake &&  is_act  && !active_routing[0];
    assign act_we_1    = handshake &&  is_act  &&  active_routing[0];

endmodule
