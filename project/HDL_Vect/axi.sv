`timescale 1ns / 1ps
// AXI4-Stream input buffer for the 128-wide vector MAC.
//
// tuser encoding (same as systolic design):
//   tuser[1]=0 → weight SRAM,  tuser[1]=1 → activation SRAM
//   tuser[0]=0 → ping (buf 0), tuser[0]=1 → pong (buf 1)
//
// Weight writes:
//   Each weight SRAM row is VEC_SIZE*WT_WIDTH = 2048 bits wide.
//   AXI is 512 bits → 4 beats assemble one row.
//   Beats 0..2 are stored in wt_data_reg; beat 3 is forwarded directly from
//   s_axis_tdata so the full 2048-bit word is valid when we assert wt_we.
//
// Activation writes:
//   The entire K-element activation vector fits in one 512-bit beat.
//   One beat per ping/pong slot (tlast after the single beat).
module axis_to_vect_buffer #(
    parameter AXI_DATA_WIDTH = 512,
    parameter TUSER_WIDTH    = 2,
    parameter VEC_SIZE       = 128,
    parameter WT_WIDTH       = 16,
    parameter K_DEPTH        = 32,
    parameter K_ADDR_WIDTH   = $clog2(K_DEPTH) // 5 for K_DEPTH=32
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream slave
    input  logic [AXI_DATA_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_WIDTH-1:0]    s_axis_tuser,
    input  logic                      s_axis_tvalid,
    output logic                      s_axis_tready,
    input  logic                      s_axis_tlast,

    // Weight SRAMs (ping / pong) – 2048-bit wide rows
    output logic                           wt_we_0,
    output logic                           wt_we_1,
    output logic [K_ADDR_WIDTH-1:0]        wt_addr,
    output logic [VEC_SIZE*WT_WIDTH-1:0]   wt_data,

    // Activation SRAMs (ping / pong) – 512-bit rows, addr always 0
    output logic                           act_we_0,
    output logic                           act_we_1,
    output logic [AXI_DATA_WIDTH-1:0]      act_data
);

    // 4 AXI beats per weight SRAM row
    localparam BEATS_PER_ROW = (VEC_SIZE * WT_WIDTH) / AXI_DATA_WIDTH; // 4
    localparam BEAT_BITS     = $clog2(BEATS_PER_ROW);                   // 2

    // write_ptr: global beat count within the current AXI packet.
    // Widened to 7 bits to cover K_DEPTH * BEATS_PER_ROW = 128 beats.
    logic [6:0]             write_ptr;
    logic [TUSER_WIDTH-1:0] routing;   // tuser latched from first beat

    logic axis_handshake;
    assign axis_handshake  = s_axis_tvalid && s_axis_tready;
    assign s_axis_tready   = rst_n;

    // Active routing: bypass latch on beat 0 so WE fires immediately.
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

    // ── Weight data assembly ──────────────────────────────────────────────
    // Accumulate beats 0..2 into wt_data_reg; beat 3 is taken directly from
    // s_axis_tdata so the full row is combinationally valid at write time.
    logic [VEC_SIZE*WT_WIDTH-1:0] wt_data_reg;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            wt_data_reg <= '0;
        else if (axis_handshake && !active_routing[1]) begin
            case (write_ptr[BEAT_BITS-1:0])
                2'b00: wt_data_reg[0*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] <= s_axis_tdata;
                2'b01: wt_data_reg[1*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] <= s_axis_tdata;
                2'b10: wt_data_reg[2*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] <= s_axis_tdata;
                default: ; // beat 3: forwarded combinationally, not registered
            endcase
        end
    end

    // Combine registered beats 0-2 with live beat 3 for the SRAM write word.
    assign wt_data[0*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] = wt_data_reg[0*AXI_DATA_WIDTH +: AXI_DATA_WIDTH];
    assign wt_data[1*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] = wt_data_reg[1*AXI_DATA_WIDTH +: AXI_DATA_WIDTH];
    assign wt_data[2*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] = wt_data_reg[2*AXI_DATA_WIDTH +: AXI_DATA_WIDTH];
    assign wt_data[3*AXI_DATA_WIDTH +: AXI_DATA_WIDTH] = s_axis_tdata; // always live

    // Weight SRAM row address = beat_index / BEATS_PER_ROW
    assign wt_addr = K_ADDR_WIDTH'(write_ptr >> BEAT_BITS);

    // Write enable fires on the 4th (last) beat of each weight row.
    logic do_wt_write;
    assign do_wt_write = axis_handshake
                      && !active_routing[1]
                      && (write_ptr[BEAT_BITS-1:0] == BEAT_BITS'(BEATS_PER_ROW - 1));

    assign wt_we_0 = do_wt_write && !active_routing[0]; // ping
    assign wt_we_1 = do_wt_write &&  active_routing[0]; // pong

    // ── Activation passthrough (one beat per packet) ──────────────────────
    assign act_data = s_axis_tdata;
    assign act_we_0 = axis_handshake &&  active_routing[1] && !active_routing[0];
    assign act_we_1 = axis_handshake &&  active_routing[1] &&  active_routing[0];

endmodule
