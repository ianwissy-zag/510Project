`timescale 1ns / 1ps
// AXI4-Stream input interface for the VEC_SIZE-wide vector MAC accelerator.
//
// Receives weight and activation data over a 512-bit AXI4-Stream slave port
// and routes it to the appropriate weight or activation SRAM bank.
//
// Reset: asynchronous active-low (negedge rst_n). s_axis_tready deasserts
//        during reset, preventing any data from being accepted.
//
// tuser routing (latched from first beat of each packet):
//   tuser[1:0] = 2'b00 → weight ping SRAM  (wt_we_0)
//   tuser[1:0] = 2'b01 → weight pong SRAM  (wt_we_1)
//   tuser[1:0] = 2'b10 → activation ping   (act_we_0)
//   tuser[1:0] = 2'b11 → activation pong   (act_we_1)
//
// Weight writes:
//   BEATS_PER_ROW = VEC_SIZE*WT_WIDTH / AXI_DATA_WIDTH beats assemble one row.
//   Beats 0..BEATS_PER_ROW-2 are registered; the last beat is forwarded
//   combinationally so the full row is valid when wt_we fires.
//   wt_addr increments automatically each BEATS_PER_ROW beats.
//
// Activation writes:
//   One 512-bit beat per ping/pong slot (tlast asserted on the single beat).
//
// Port list:
//   clk            IN   Clock
//   rst_n          IN   Async active-low reset
//
//   s_axis_tdata   IN   [AXI_DATA_WIDTH-1:0]  Incoming data beat (512 bits)
//   s_axis_tuser   IN   [TUSER_WIDTH-1:0]      Routing tag — sampled on beat 0,
//                                               held for the rest of the packet
//   s_axis_tvalid  IN   AXI-S handshake: data valid
//   s_axis_tready  OUT  AXI-S handshake: module ready to accept (= rst_n)
//   s_axis_tlast   IN   Last beat of the current packet
//
//   wt_we_0        OUT  Write enable for weight ping SRAM (fires on last beat)
//   wt_we_1        OUT  Write enable for weight pong SRAM (fires on last beat)
//   wt_addr        OUT  [K_ADDR_WIDTH-1:0]     Row address driven into weight SRAMs
//   wt_data        OUT  [VEC_SIZE*WT_WIDTH-1:0] Full weight row assembled from beats
//
//   act_we_0       OUT  Write enable for activation ping SRAM
//   act_we_1       OUT  Write enable for activation pong SRAM
//   act_data       OUT  [AXI_DATA_WIDTH-1:0]   Activation data (passthrough from tdata)
module axi_interface #(
    parameter AXI_DATA_WIDTH = 512,
    parameter TUSER_WIDTH    = 2,
    parameter VEC_SIZE       = 512,
    parameter WT_WIDTH       = 16,
    parameter K_DEPTH        = 32,
    parameter K_ADDR_WIDTH   = $clog2(K_DEPTH)
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave
    input  logic [AXI_DATA_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_WIDTH-1:0]    s_axis_tuser,
    input  logic                      s_axis_tvalid,
    output logic                      s_axis_tready,
    input  logic                      s_axis_tlast,

    // Weight SRAMs (ping / pong)
    output logic                           wt_we_0,
    output logic                           wt_we_1,
    output logic [K_ADDR_WIDTH-1:0]        wt_addr,
    output logic [VEC_SIZE*WT_WIDTH-1:0]   wt_data,

    // Activation SRAMs (ping / pong) – one beat per slot
    output logic                           act_we_0,
    output logic                           act_we_1,
    output logic [AXI_DATA_WIDTH-1:0]      act_data
);

    localparam BEATS_PER_ROW = (VEC_SIZE * WT_WIDTH) / AXI_DATA_WIDTH;
    localparam BEAT_BITS     = $clog2(BEATS_PER_ROW);

    // write_ptr counts beats within the current AXI packet.
    // Width covers K_DEPTH × BEATS_PER_ROW total beats.
    localparam PTR_BITS = $clog2(K_DEPTH * BEATS_PER_ROW) + 1;
    logic [PTR_BITS-1:0]  write_ptr;
    logic [TUSER_WIDTH-1:0] routing;

    logic axis_handshake;
    assign axis_handshake = s_axis_tvalid && s_axis_tready;
    assign s_axis_tready  = rst_n;

    logic [TUSER_WIDTH-1:0] active_routing;
    assign active_routing = (write_ptr == '0) ? s_axis_tuser : routing;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) routing <= '0;
        else if (axis_handshake && write_ptr == '0) routing <= s_axis_tuser;
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) write_ptr <= '0;
        else if (axis_handshake)
            write_ptr <= s_axis_tlast ? '0 : write_ptr + 1'b1;
    end

    // ── Weight data assembly ──────────────────────────────────────────────────
    // Register beats 0..(BEATS_PER_ROW-2); last beat forwarded combinationally
    // so the full row is valid when wt_we fires.
    logic [VEC_SIZE*WT_WIDTH-1:0] wt_data_reg;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            wt_data_reg <= '0;
        else if (axis_handshake && !active_routing[1]) begin
            // Store all beats except the last into the register
            if (write_ptr[BEAT_BITS-1:0] != BEAT_BITS'(BEATS_PER_ROW - 1))
                wt_data_reg[write_ptr[BEAT_BITS-1:0] * AXI_DATA_WIDTH +: AXI_DATA_WIDTH]
                    <= s_axis_tdata;
        end
    end

    // Output: registered beats with live last beat substituted in
    always_comb begin
        wt_data = wt_data_reg;
        wt_data[(BEATS_PER_ROW-1) * AXI_DATA_WIDTH +: AXI_DATA_WIDTH] = s_axis_tdata;
    end

    // Weight SRAM row address = beat_index / BEATS_PER_ROW
    assign wt_addr = K_ADDR_WIDTH'(write_ptr >> BEAT_BITS);

    // Write enable fires on the last beat of each weight row
    logic do_wt_write;
    assign do_wt_write = axis_handshake
                      && !active_routing[1]
                      && (write_ptr[BEAT_BITS-1:0] == BEAT_BITS'(BEATS_PER_ROW - 1));

    assign wt_we_0 = do_wt_write && !active_routing[0];
    assign wt_we_1 = do_wt_write &&  active_routing[0];

    // ── Activation passthrough (one beat per packet) ──────────────────────────
    assign act_data = s_axis_tdata;
    assign act_we_0 = axis_handshake &&  active_routing[1] && !active_routing[0];
    assign act_we_1 = axis_handshake &&  active_routing[1] &&  active_routing[0];

endmodule
