`timescale 1ns / 1ps
/* verilator lint_off DECLFILENAME */
// tuser encoding (2 bits, carried on every beat):
//   tuser[1] : destination  – 0 = weight SRAM,  1 = activation SRAM
//   tuser[0] : buffer select – 0 = ping (buf 0), 1 = pong (buf 1)
//
//   2'b00 = weight ping   2'b01 = weight pong
//   2'b10 = act ping      2'b11 = act pong

module axis_to_pingpong_buffer #(
    parameter AXI_DATA_WIDTH = 512, // 32 elements of 16-bit BF16
    parameter TUSER_WIDTH    = 2,   // routing metadata embedded in stream
    parameter SRAM_DEPTH     = 32,  // 32 rows to fill a 32x32 array
    parameter ADDR_WIDTH     = $clog2(SRAM_DEPTH) // 5 bits for 32 depth
)(
    input  logic clk,
    input  logic rst_n,

    // AXI4-Stream Slave Interface
    input  logic [AXI_DATA_WIDTH-1:0] s_axis_tdata,
    input  logic [TUSER_WIDTH-1:0]    s_axis_tuser,  // destination routing
    input  logic                      s_axis_tvalid,
    output logic                      s_axis_tready,
    input  logic                      s_axis_tlast, // Indicates end of a 32-beat packet
    
    // SRAM Interface: Weights (Ping and Pong)
    output logic                      wt_we_0,   // Write Enable for Weight Ping
    output logic                      wt_we_1,   // Write Enable for Weight Pong
    output logic [ADDR_WIDTH-1:0]     wt_addr,
    output logic [AXI_DATA_WIDTH-1:0] wt_data,
    
    // SRAM Interface: Activations (Ping and Pong)
    output logic                      act_we_0,  // Write Enable for Act Ping
    output logic                      act_we_1,  // Write Enable for Act Pong
    output logic [ADDR_WIDTH-1:0]     act_addr,
    output logic [AXI_DATA_WIDTH-1:0] act_data
);

    logic [ADDR_WIDTH-1:0]  write_ptr;
    logic [TUSER_WIDTH-1:0] routing;     // tuser latched from first beat of packet

    logic axis_handshake;
    assign axis_handshake = s_axis_tvalid && s_axis_tready;

    assign s_axis_tready = rst_n;

    // -------------------------------------------------------------------------
    // Routing latch – capture tuser on the first beat (write_ptr == 0).
    // active_routing bypasses the latch on beat 0 so the correct write enable
    // fires immediately without waiting for the register to update.
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            routing <= '0;
        else if (axis_handshake && write_ptr == '0)
            routing <= s_axis_tuser;
    end

    logic [TUSER_WIDTH-1:0] active_routing;
    assign active_routing = (write_ptr == '0) ? s_axis_tuser : routing;

    // -------------------------------------------------------------------------
    // Address Counter
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            write_ptr <= '0;
        else if (axis_handshake)
            write_ptr <= s_axis_tlast ? '0 : write_ptr + 1'b1;
    end

    // -------------------------------------------------------------------------
    // Data Routing and Write Enables
    // The data and address lines are shared; Write Enables act as the demux.
    // -------------------------------------------------------------------------
    assign wt_data  = s_axis_tdata;
    assign act_data = s_axis_tdata;

    assign wt_addr  = write_ptr;
    assign act_addr = write_ptr;

    always_comb begin
        wt_we_0  = 1'b0;
        wt_we_1  = 1'b0;
        act_we_0 = 1'b0;
        act_we_1 = 1'b0;

        if (axis_handshake) begin
            case (active_routing)
                2'b00: wt_we_0  = 1'b1;  // weight ping
                2'b01: wt_we_1  = 1'b1;  // weight pong
                2'b10: act_we_0 = 1'b1;  // activation ping
                2'b11: act_we_1 = 1'b1;  // activation pong
            endcase
        end
    end

endmodule
