`timescale 1ns / 1ps

// Controller for the 128-wide vector MAC accelerator.
//
// One "tile" = read all K_DEPTH weight rows sequentially, multiply each by
// the corresponding activation scalar, accumulate into 128 partial sums,
// then capture the result.
//
// Pipeline timing:
//   PREFETCH (1 cycle):
//     - Assert act_re so act_rdata is valid at the start of COMPUTE.
//     - Drive wt_addr=0 so wt_rdata is valid at the start of COMPUTE.
//     - Assert load_mac so accumulators are seeded at the PREFETCH posedge.
//     - Latch act_rdata into act_reg at the PREFETCH→COMPUTE transition.
//   COMPUTE (K_DEPTH cycles, counter 0..K_DEPTH-1):
//     - Cycle k uses wt_rdata=wt_mem[k] (from addr driven in previous cycle).
//     - Cycle k uses act_reg[k*ACT_WIDTH +: ACT_WIDTH] (latched activation).
//     - wt_addr = counter+1 (lookahead for next cycle; wraps harmlessly on last).
//   CAPTURE:
//     - Capture accum_reg from psum_out for inter-tile use.
//     - Write output_sram[0] if last_tile.
//   DONE_ST: pulse done for one cycle, return to IDLE.
module controller #(
    parameter VEC_SIZE      = 128,
    parameter ACT_WIDTH     = 16,
    parameter WT_WIDTH      = 16,
    parameter PSUM_WIDTH    = 32,
    parameter K_DEPTH       = 32,
    parameter K_ADDR_WIDTH  = $clog2(K_DEPTH),  // 5
    parameter OUT_ADDR_WIDTH = 1                 // output SRAM ADDR_WIDTH
)(
    input  logic clk,
    input  logic rst_n,

    input  logic start,
    input  logic act_buf_sel,  // 0=ping, 1=pong
    input  logic first_tile,
    input  logic last_tile,
    output logic done,

    // Weight SRAM (single-port, read during COMPUTE)
    output logic [K_ADDR_WIDTH-1:0]           wt_addr,
    input  logic [VEC_SIZE*WT_WIDTH-1:0]       wt_rdata,

    // Activation SRAMs (ping/pong dual-port, read during PREFETCH/COMPUTE)
    output logic                              act_re_0,
    output logic                              act_re_1,
    output logic [0:0]                        act_raddr,  // always 0
    input  logic [K_DEPTH*ACT_WIDTH-1:0]       act_rdata_0,  // 512-bit packed vector
    input  logic [K_DEPTH*ACT_WIDTH-1:0]       act_rdata_1,

    // Vec MAC array control
    output logic                              load_mac,
    output logic                              mac_en,
    output logic [ACT_WIDTH-1:0]              act_in,     // scalar to MAC array
    output logic [VEC_SIZE*WT_WIDTH-1:0]      wt_in,      // row to MAC array
    output logic [VEC_SIZE*PSUM_WIDTH-1:0]    psum_seed,  // inter-tile seed
    input  logic [VEC_SIZE*PSUM_WIDTH-1:0]    psum_out,

    // Output SRAM write port
    output logic                              out_we,
    output logic [OUT_ADDR_WIDTH-1:0]         out_waddr,
    output logic [VEC_SIZE*PSUM_WIDTH-1:0]    out_wdata
);

    localparam ACT_PACKED_WIDTH = K_DEPTH * ACT_WIDTH; // 512

    typedef enum logic [2:0] {
        IDLE, PREFETCH, COMPUTE, CAPTURE, DONE_ST
    } state_t;

    state_t                        state;
    logic [K_ADDR_WIDTH-1:0]       counter;   // 0..K_DEPTH-1
    logic [ACT_PACKED_WIDTH-1:0]   act_reg;   // latched activation vector
    logic [VEC_SIZE*PSUM_WIDTH-1:0] accum_reg; // inter-tile accumulator

    // ── State machine ────────────────────────────────────────────────────
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state    <= IDLE;
            counter  <= '0;
            act_reg  <= '0;
            accum_reg <= '0;
        end else begin
            case (state)
                IDLE:
                    if (start) begin
                        state   <= PREFETCH;
                        counter <= '0;
                    end

                PREFETCH: begin
                    // Capture activation vector (valid because act_re was high
                    // in both IDLE and this PREFETCH cycle).
                    act_reg  <= act_buf_sel ? act_rdata_1 : act_rdata_0;
                    state    <= COMPUTE;
                    counter  <= '0;
                end

                COMPUTE:
                    if (counter == K_ADDR_WIDTH'(K_DEPTH - 1)) begin
                        state   <= CAPTURE;
                        counter <= '0;
                    end else
                        counter <= counter + 1'b1;

                CAPTURE: begin
                    accum_reg <= psum_out;  // save for next tile
                    state     <= DONE_ST;
                end

                DONE_ST:
                    state <= IDLE;

                default:
                    state <= IDLE;
            endcase
        end
    end

    // ── Weight SRAM address ───────────────────────────────────────────────
    // PREFETCH: drive addr=0 so wt_rdata=wt_mem[0] is ready at COMPUTE cycle 0.
    // COMPUTE:  drive addr=counter+1 (lookahead; data from counter arrives this cycle).
    assign wt_addr = (state == PREFETCH) ? '0
                   : (state == COMPUTE)  ? K_ADDR_WIDTH'(counter + 1'b1)
                   :                       '0;

    // ── Activation SRAM ───────────────────────────────────────────────────
    // Assert re in IDLE (pre-charge) and PREFETCH so data is ready for latch.
    assign act_raddr = '0;  // single-row activation buffer
    assign act_re_0  = ((state == IDLE) || (state == PREFETCH)) && !act_buf_sel;
    assign act_re_1  = ((state == IDLE) || (state == PREFETCH)) &&  act_buf_sel;

    // ── MAC array control ─────────────────────────────────────────────────
    // load_mac: fire in PREFETCH so accumulators are seeded before COMPUTE.
    // mac_en:   fire for all K_DEPTH COMPUTE cycles.
    assign load_mac   = (state == PREFETCH);
    assign mac_en     = (state == COMPUTE);
    assign psum_seed  = first_tile ? '0 : accum_reg;

    // Activation scalar: index into the latched packed vector.
    assign act_in = ACT_WIDTH'(act_reg[counter * ACT_WIDTH +: ACT_WIDTH]);

    // Weight row: straight from the registered SRAM output.
    assign wt_in  = wt_rdata;

    // ── Output SRAM ───────────────────────────────────────────────────────
    assign out_we    = (state == CAPTURE) && last_tile;
    assign out_waddr = '0;
    assign out_wdata = psum_out;

    // ── Done pulse ────────────────────────────────────────────────────────
    assign done = (state == DONE_ST);

endmodule
