`timescale 1ns / 1ps

module axis_to_pingpong_buffer #(
    parameter AXI_DATA_WIDTH = 512, // 32 elements of 16-bit BF16
    parameter SRAM_DEPTH     = 32,  // 32 rows to fill a 32x32 array
    parameter ADDR_WIDTH     = $clog2(SRAM_DEPTH) // 5 bits for 32 depth
)(
    input  logic clk,
    input  logic rst_n,
    
    // Control interface (from a lightweight AXI-Lite config register)
    input  logic       cfg_dest_is_weight, // 1 = Writing Weights, 0 = Writing Activations
    input  logic       cfg_ping_pong_sel,  // 0 = Write to Ping (Buffer 0), 1 = Write to Pong (Buffer 1)
    
    // AXI4-Stream Slave Interface
    input  logic [AXI_DATA_WIDTH-1:0] s_axis_tdata,
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

    // Internal write address counter
    logic [ADDR_WIDTH-1:0] write_ptr;
    
    // Handshake condition: Data is transferred when both valid and ready are high
    logic axis_handshake;
    assign axis_handshake = s_axis_tvalid && s_axis_tready;

    // We are always ready to receive data unless we are held in reset
    // (Assuming our SRAMs can write in 1 cycle, we don't need backpressure here)
    assign s_axis_tready = rst_n; 

    // -------------------------------------------------------------------------
    // Address Counter Logic
    // -------------------------------------------------------------------------
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            write_ptr <= '0;
        end else begin
            if (axis_handshake) begin
                if (s_axis_tlast) begin
                    // Reset pointer at the end of the packet
                    write_ptr <= '0;
                end else begin
                    // Increment pointer to write next line to SRAM
                    write_ptr <= write_ptr + 1'b1;
                end
            end
        end
    end

    // -------------------------------------------------------------------------
    // Data Routing and Write Enables
    // -------------------------------------------------------------------------
    // The data and address lines are shared; the Write Enables act as the demux
    
    assign wt_data  = s_axis_tdata;
    assign act_data = s_axis_tdata;
    
    assign wt_addr  = write_ptr;
    assign act_addr = write_ptr;

    always_comb begin
        // Default everything to zero
        wt_we_0  = 1'b0;
        wt_we_1  = 1'b0;
        act_we_0 = 1'b0;
        act_we_1 = 1'b0;
        
        // Only assert write enable on a valid AXI handshake
        if (axis_handshake) begin
            if (cfg_dest_is_weight) begin
                // Routing to Weight Buffers
                if (cfg_ping_pong_sel == 1'b0) wt_we_0 = 1'b1;
                else                           wt_we_1 = 1'b1;
            end else begin
                // Routing to Activation Buffers
                if (cfg_ping_pong_sel == 1'b0) act_we_0 = 1'b1;
                else                           act_we_1 = 1'b1;
            end
        end
    end

endmodule
