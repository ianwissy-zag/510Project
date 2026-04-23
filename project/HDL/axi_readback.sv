`timescale 1ns / 1ps

// AXI4-Stream master that drains output_sram back to the host.
//
// Each output_sram row is 1024 bits (32 × 32-bit psums).  The AXI bus is
// 512 bits wide, so each row produces two beats:
//   beat 0 → psum[0..15]   (lower half,  bits [511:0])
//   beat 1 → psum[16..31]  (upper half,  bits [1023:512]; tlast on last row)
//
// Timing note: output_sram has a registered read port (rdata updates one
// cycle after re is asserted).  This module asserts sram_re in FETCH and
// transitions to BEAT0 on the NEXT cycle when sram_rdata is valid.
// sram_re stays deasserted during BEAT0/BEAT1 so sram_rdata holds steady
// even if tready back-pressure stalls the beat for multiple cycles.
module axi_readback #(
    parameter PSUM_WIDTH = 32,
    parameter ARRAY_SIZE = 32,
    parameter DATA_WIDTH = PSUM_WIDTH * ARRAY_SIZE,  // 1024
    parameter AXI_WIDTH  = DATA_WIDTH / 2,           // 512
    parameter DEPTH      = 32,
    parameter ADDR_WIDTH = $clog2(DEPTH)
)(
    input  logic clk,
    input  logic rst_n,

    input  logic start,   // pulse: begin streaming all DEPTH rows
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

    typedef enum logic [2:0] {
        IDLE, FETCH, BEAT0, BEAT1
    } state_t;

    state_t              state;
    logic [ADDR_WIDTH:0] row;       // 0..DEPTH; one extra bit to detect wrap

    logic last_row;
    assign last_row = (row == (ADDR_WIDTH+1)'(DEPTH - 1));

    assign busy = (state != IDLE);

    // ── State machine ─────────────────────────────────────────────────────
    // FETCH: assert sram_re for one cycle; output_sram registers rdata at
    //        the SAME posedge, so sram_rdata is valid in BEAT0 (next cycle).
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            row   <= '0;
        end else begin
            case (state)
                IDLE: begin
                    row <= '0;
                    if (start) state <= FETCH;
                end

                FETCH: state <= BEAT0;   // sram_rdata valid next cycle

                BEAT0:
                    if (m_axis_tready) state <= BEAT1;

                BEAT1:
                    if (m_axis_tready) begin
                        if (last_row)
                            state <= IDLE;
                        else begin
                            row   <= row + 1'b1;
                            state <= FETCH;
                        end
                    end

                default: state <= IDLE;
            endcase
        end
    end

    // ── SRAM read control ─────────────────────────────────────────────────
    // re is high only in FETCH; sram_rdata is stable in BEAT0/BEAT1.
    assign sram_re    = (state == FETCH);
    assign sram_raddr = ADDR_WIDTH'(row);

    // ── AXI-S output ─────────────────────────────────────────────────────
    // Drive directly from the SRAM's registered output (no extra FF needed).
    assign m_axis_tvalid = (state == BEAT0) || (state == BEAT1);
    assign m_axis_tdata  = (state == BEAT0) ? sram_rdata[AXI_WIDTH-1:0]
                                             : sram_rdata[DATA_WIDTH-1:AXI_WIDTH];
    assign m_axis_tlast  = (state == BEAT1) && last_row;

endmodule
