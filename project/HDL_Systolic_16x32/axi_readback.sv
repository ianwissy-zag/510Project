`timescale 1ns / 1ps

// AXI4-Stream readback — streams COLS FP32 partial sums as 512-bit beats.
// COLS=32 × 32-bit = 1024 bits → 2 × 512-bit beats.
//
// m_axis_tlast is combinational so it is valid in the same cycle as the
// last beat's data — avoids the 1-cycle registered lag.

module axi_readback #(
    parameter COLS       = 32,
    parameter PSUM_WIDTH = 32,
    parameter AXI_WIDTH  = 512
)(
    input  logic clk,
    input  logic rst_n,
    input  logic start,
    output logic busy,
    input  logic [COLS*PSUM_WIDTH-1:0] sram_rdata,
    output logic [AXI_WIDTH-1:0] m_axis_tdata,
    output logic                 m_axis_tvalid,
    input  logic                 m_axis_tready,
    output logic                 m_axis_tlast
);
    localparam TOTAL_BITS = COLS * PSUM_WIDTH;      // 1024
    localparam BEATS      = TOTAL_BITS / AXI_WIDTH; // 2
    localparam BEAT_BITS  = $clog2(BEATS);           // 1

    typedef enum logic { IDLE, STREAM } state_t;
    state_t               state;
    logic [BEAT_BITS-1:0] beat;
    logic [TOTAL_BITS-1:0] buf_reg;

    assign busy        = (state != IDLE);
    // combinational outputs — valid in the same cycle as the data
    assign m_axis_tdata  = buf_reg[beat * AXI_WIDTH +: AXI_WIDTH];
    assign m_axis_tvalid = (state == STREAM);
    assign m_axis_tlast  = (state == STREAM) && (beat == BEAT_BITS'(BEATS - 1));

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state   <= IDLE;
            beat    <= '0;
            buf_reg <= '0;
        end else begin
            case (state)
                IDLE: begin
                    if (start) begin
                        buf_reg <= sram_rdata;
                        beat    <= '0;
                        state   <= STREAM;
                    end
                end

                STREAM: begin
                    if (m_axis_tready) begin
                        if (beat == BEAT_BITS'(BEATS - 1))
                            state <= IDLE;
                        else
                            beat <= beat + 1'b1;
                    end
                end
            endcase
        end
    end
endmodule
