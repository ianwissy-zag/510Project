`timescale 1ns / 1ps

// AXI4-Stream master that drains output_sram back to the host.
//
// The output SRAM row is VEC_SIZE * PSUM_WIDTH = 4096 bits wide.
// The AXI bus is 512 bits, so each row produces BEATS_PER_ROW = 8 beats:
//   beat 0 → psum[0..15]    bits [511:0]
//   beat 1 → psum[16..31]   bits [1023:512]
//   ...
//   beat 7 → psum[112..127] bits [4095:3584]  (tlast on last row)
//
// Only DEPTH rows are streamed (depth=2 by default, but only row 0 is written).
// FETCH state asserts sram_re for one cycle; sram_rdata is valid in STREAM.
module axi_readback #(
    parameter PSUM_WIDTH    = 32,
    parameter VEC_SIZE      = 128,
    parameter DATA_WIDTH    = PSUM_WIDTH * VEC_SIZE,  // 4096
    parameter AXI_WIDTH     = 512,
    parameter BEATS_PER_ROW = DATA_WIDTH / AXI_WIDTH, // 8
    parameter N_ROWS        = 1,     // number of rows to stream (≤ DEPTH)
    parameter DEPTH         = 2,
    parameter ADDR_WIDTH    = $clog2(DEPTH)            // 1
)(
    input  logic clk,
    input  logic rst_n,

    input  logic start,
    output logic busy,

    // Output SRAM read port
    output logic                   sram_re,
    output logic [ADDR_WIDTH-1:0]  sram_raddr,
    input  logic [DATA_WIDTH-1:0]  sram_rdata,  // registered output

    // AXI4-Stream master
    output logic [AXI_WIDTH-1:0]   m_axis_tdata,
    output logic                   m_axis_tvalid,
    input  logic                   m_axis_tready,
    output logic                   m_axis_tlast
);

    localparam BEAT_BITS = $clog2(BEATS_PER_ROW); // 3 for 8 beats

    typedef enum logic [1:0] {
        IDLE, FETCH, STREAM
    } state_t;

    state_t               state;
    logic [ADDR_WIDTH:0]  row;      // one extra bit to detect wrap
    logic [BEAT_BITS-1:0] beat;     // 0..BEATS_PER_ROW-1

    logic last_row;
    logic last_beat;
    assign last_row  = (row == (ADDR_WIDTH+1)'(N_ROWS - 1));
    assign last_beat = (beat == BEAT_BITS'(BEATS_PER_ROW - 1));

    assign busy = (state != IDLE);

    // ── State machine ─────────────────────────────────────────────────────
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            row   <= '0;
            beat  <= '0;
        end else begin
            case (state)
                IDLE: begin
                    row  <= '0;
                    beat <= '0;
                    if (start) state <= FETCH;
                end

                FETCH:
                    state <= STREAM; // sram_rdata valid next cycle

                STREAM:
                    if (m_axis_tready) begin
                        if (last_beat) begin
                            beat <= '0;
                            if (last_row)
                                state <= IDLE;
                            else begin
                                row   <= row + 1'b1;
                                state <= FETCH;
                            end
                        end else
                            beat <= beat + 1'b1;
                    end

                default: state <= IDLE;
            endcase
        end
    end

    // ── SRAM read control ─────────────────────────────────────────────────
    assign sram_re    = (state == FETCH);
    assign sram_raddr = ADDR_WIDTH'(row);

    // ── AXI-S output ─────────────────────────────────────────────────────
    assign m_axis_tvalid = (state == STREAM);
    assign m_axis_tdata  = sram_rdata[beat * AXI_WIDTH +: AXI_WIDTH];
    assign m_axis_tlast  = (state == STREAM) && last_beat && last_row;

endmodule
