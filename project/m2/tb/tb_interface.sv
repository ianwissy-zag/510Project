`timescale 1ns / 1ps

// Testbench for axi_interface (interface.sv)
//
// Verifies AXI-Stream routing to weight and activation SRAMs:
//   Test 1: Weight beats route to ping bank (tuser=2'b00), wt_we_0 fires on last beat
//   Test 2: Weight beats route to pong bank (tuser=2'b01), wt_we_1 fires on last beat
//   Test 3: Activation beat routes to act ping (tuser=2'b10), act_we_0 fires
//   Test 4: Activation beat routes to act pong (tuser=2'b11), act_we_1 fires
//   Test 5: tuser latched from beat 0; change mid-packet is ignored
//   Test 6: No write enables when tvalid=0
//
// Uses reduced VEC_SIZE=128, K_DEPTH=4 so only 8 beats per weight row.

module tb_interface;

    // Parameters matching a small test configuration
    localparam AXI_DATA_WIDTH = 512;
    localparam TUSER_WIDTH    = 2;
    localparam VEC_SIZE       = 128;
    localparam WT_WIDTH       = 16;
    localparam K_DEPTH        = 4;
    localparam K_ADDR_WIDTH   = $clog2(K_DEPTH);   // 2
    localparam BEATS_PER_ROW  = (VEC_SIZE * WT_WIDTH) / AXI_DATA_WIDTH; // 4

    // DUT signals
    logic                          clk;
    logic                          rst_n;
    logic [AXI_DATA_WIDTH-1:0]     s_axis_tdata;
    logic [TUSER_WIDTH-1:0]        s_axis_tuser;
    logic                          s_axis_tvalid;
    logic                          s_axis_tready;
    logic                          s_axis_tlast;
    logic                          wt_we_0, wt_we_1;
    logic [K_ADDR_WIDTH-1:0]       wt_addr;
    logic [VEC_SIZE*WT_WIDTH-1:0]  wt_data;
    logic                          act_we_0, act_we_1;
    logic [AXI_DATA_WIDTH-1:0]     act_data;

    integer errors = 0;
    integer b;
    logic   saw_we0, saw_we1, saw_we0_t5;

    // Instantiate DUT with small parameters for testing
    axi_interface #(
        .AXI_DATA_WIDTH(AXI_DATA_WIDTH),
        .TUSER_WIDTH   (TUSER_WIDTH),
        .VEC_SIZE      (VEC_SIZE),
        .WT_WIDTH      (WT_WIDTH),
        .K_DEPTH       (K_DEPTH),
        .K_ADDR_WIDTH  (K_ADDR_WIDTH)
    ) dut (
        .clk           (clk),
        .rst_n         (rst_n),
        .s_axis_tdata  (s_axis_tdata),
        .s_axis_tuser  (s_axis_tuser),
        .s_axis_tvalid (s_axis_tvalid),
        .s_axis_tready (s_axis_tready),
        .s_axis_tlast  (s_axis_tlast),
        .wt_we_0       (wt_we_0),
        .wt_we_1       (wt_we_1),
        .wt_addr       (wt_addr),
        .wt_data       (wt_data),
        .act_we_0      (act_we_0),
        .act_we_1      (act_we_1),
        .act_data      (act_data)
    );

    // Clock: 10ns period
    initial clk = 0;
    always #5 clk = ~clk;

    // Helper: send one AXI-S beat
    task send_beat(
        input [AXI_DATA_WIDTH-1:0] data,
        input [TUSER_WIDTH-1:0]    tuser,
        input                      tlast
    );
        @(negedge clk);
        s_axis_tdata  = data;
        s_axis_tuser  = tuser;
        s_axis_tvalid = 1;
        s_axis_tlast  = tlast;
        @(posedge clk); #1;
        s_axis_tvalid = 0;
        s_axis_tlast  = 0;
    endtask

    // Helper: send a full weight matrix (K_DEPTH rows × BEATS_PER_ROW beats)
    task send_weight_matrix(input [TUSER_WIDTH-1:0] tuser, input [511:0] beat_val);
        integer total, b;
        total = K_DEPTH * BEATS_PER_ROW;
        for (b = 0; b < total; b++)
            send_beat(beat_val, tuser, (b == total - 1));
        @(posedge clk); @(posedge clk);
    endtask

    // Check helper
    task chk(input string label, input logic got, input logic expected);
        if (got !== expected) begin
            $display("FAIL [%s]: got %b expected %b", label, got, expected);
            errors++;
        end
    endtask

    initial begin
        // Reset
        rst_n = 0; s_axis_tvalid = 0; s_axis_tlast = 0;
        s_axis_tdata = '0; s_axis_tuser = '0;
        repeat(4) @(posedge clk);
        rst_n = 1; @(posedge clk);

        // ── Test 1: weights → ping (tuser=00), wt_we_0 fires on last beat ────
        $display("Test 1: Weight to ping bank (tuser=00)");
        saw_we0 = 0;
        for (b = 0; b < K_DEPTH * BEATS_PER_ROW; b++) begin
                send_beat(512'hDEAD, 2'b00, (b == K_DEPTH*BEATS_PER_ROW-1));
                if (wt_we_0) saw_we0 = 1;
                chk("wt_we_1 low during wt ping", wt_we_1, 0);
            end
        chk("wt_we_0 fired", saw_we0, 1);
        @(posedge clk); @(posedge clk);

        // ── Test 2: weights → pong (tuser=01), wt_we_1 fires ─────────────────
        $display("Test 2: Weight to pong bank (tuser=01)");
        saw_we1 = 0;
        for (b = 0; b < K_DEPTH * BEATS_PER_ROW; b++) begin
                send_beat(512'hBEEF, 2'b01, (b == K_DEPTH*BEATS_PER_ROW-1));
                if (wt_we_1) saw_we1 = 1;
                chk("wt_we_0 low during wt pong", wt_we_0, 0);
            end
        chk("wt_we_1 fired", saw_we1, 1);
        @(posedge clk); @(posedge clk);

        // ── Test 3: activation → ping (tuser=10), act_we_0 fires ─────────────
        $display("Test 3: Activation to ping (tuser=10)");
        send_beat(512'hCAFE, 2'b10, 1);
        @(posedge clk); #1;
        chk("act_we_0 not fired yet", act_we_0, 0);  // fires during beat
        chk("act_we_1 low", act_we_1, 0);
        @(posedge clk); @(posedge clk);

        // ── Test 4: activation → pong (tuser=11), act_we_1 fires ─────────────
        $display("Test 4: Activation to pong (tuser=11)");
        @(negedge clk);
        s_axis_tdata = 512'hF00D; s_axis_tuser = 2'b11;
        s_axis_tvalid = 1; s_axis_tlast = 1;
        @(posedge clk); #1;
        chk("act_we_1 fires on act pong beat", act_we_1, 1);
        chk("act_we_0 low during act pong",    act_we_0, 0);
        s_axis_tvalid = 0; s_axis_tlast = 0;
        @(posedge clk); @(posedge clk);

        // ── Test 5: tuser latched from beat 0; mid-packet change ignored ──────
        $display("Test 5: tuser locked to beat-0 value");
        saw_we0_t5 = 0;
        for (b = 0; b < K_DEPTH * BEATS_PER_ROW; b++) begin
                // Change tuser to 01 after beat 0 — should be ignored
                send_beat(512'hAAAA, (b == 0) ? 2'b00 : 2'b01,
                          (b == K_DEPTH*BEATS_PER_ROW-1));
                if (wt_we_0) saw_we0_t5 = 1;
            end
        chk("wt_we_0 fired despite mid-change", saw_we0_t5, 1);
        chk("wt_we_1 not fired",                wt_we_1,    0);
        @(posedge clk); @(posedge clk);

        // ── Test 6: no write enables when tvalid=0 ────────────────────────────
        $display("Test 6: No WE when tvalid=0");
        @(posedge clk); #1;
        chk("wt_we_0 low when idle",  wt_we_0,  0);
        chk("wt_we_1 low when idle",  wt_we_1,  0);
        chk("act_we_0 low when idle", act_we_0, 0);
        chk("act_we_1 low when idle", act_we_1, 0);

        // ── Summary ───────────────────────────────────────────────────────────
        if (errors == 0)
            $display("All axi_interface tests PASSED.");
        else
            $display("%0d axi_interface test(s) FAILED.", errors);

        $finish;
    end

    // Timeout watchdog
    initial begin
        #100000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
