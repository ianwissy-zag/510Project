`timescale 1ns / 1ps

module crossbar_tb;

    // Parameters
    localparam int INPUT_WIDTH  = 8;
    localparam int OUTPUT_WIDTH = INPUT_WIDTH + 2;
    localparam     CLK_PERIOD   = 10; // ns

    // Clock and reset
    logic clk;
    logic rst;

    // DUT signals
    logic signed [INPUT_WIDTH-1:0]  acts_i    [3:0];
    logic        [3:0][3:0]         weights_i;
    logic signed [OUTPUT_WIDTH-1:0] mac_outs_o [3:0];

    // ── Clock generation ──────────────────────────────────────────────────────
    initial clk = 0;
    always #(CLK_PERIOD/2) clk = ~clk;

    // ── DUT instantiation ─────────────────────────────────────────────────────
    crossbar_mac #(
        .INPUT_WIDTH (INPUT_WIDTH),
        .OUTPUT_WIDTH(OUTPUT_WIDTH)
    ) dut (
        .clk      (clk),
        .rst      (rst),
        .acts_i   (acts_i),
        .weights_i(weights_i),
        .mac_outs_o(mac_outs_o)
    );

    // ── Test stimulus ─────────────────────────────────────────────────────────
    initial begin
        $dumpfile("mac_tb.vcd");
        $dumpvars(0, crossbar_tb);

        $display("===============================================================");
        $display("   4x4 MAC Crossbar Testbench (clocked, synchronous reset)");
        $display("   Weight Encoding: 0 = +1, 1 = -1");
        $display("===============================================================\n");

        // Initialise inputs and hold reset for 2 cycles
        rst       = 1;
        acts_i    = '{default: '0};
        weights_i = '0;
        repeat(2) @(posedge clk);
        rst = 0;

        // ------------------------------------------------------------------
        // Test Case 1: Baseline
        // Input Vector: [10, 20, 30, 40]
        // Weight Matrix: [[ 1,-1, 1,-1],
        //                 [ 1, 1,-1,-1],
        //                 [-1, 1, 1,-1],
        //                 [-1,-1,-1, 1]]
        // Expected: Col0=-40, Col1=0, Col2=-20, Col3=-20
        // ------------------------------------------------------------------
        $display("Test Case 1: Baseline");

        acts_i[0] = 8'd10;
        acts_i[1] = 8'd20;
        acts_i[2] = 8'd30;
        acts_i[3] = 8'd40;

        weights_i[0] = 4'b1010; // row 0: col3=-1,col2=+1,col1=-1,col0=+1
        weights_i[1] = 4'b1100; // row 1: col3=-1,col2=-1,col1=+1,col0=+1
        weights_i[2] = 4'b1001; // row 2: col3=-1,col2=+1,col1=+1,col0=-1
        weights_i[3] = 4'b0111; // row 3: col3=+1,col2=-1,col1=-1,col0=-1

        @(posedge clk); // latch inputs
        #1;             // small delay to let outputs settle after clock edge
        $display("Inputs: [%0d, %0d, %0d, %0d]",
                 acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: -40, Col1: 0, Col2: -20, Col3: -20");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n",
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        // ------------------------------------------------------------------
        // Test Case 2: Negative Inputs / Extreme Values
        // Input Vector: [-128, 127, -50, 0]
        // ------------------------------------------------------------------
        $display("Test Case 2: Negative Inputs / Extreme Values");

        acts_i[0] = -8'sd128;
        acts_i[1] =  8'sd127;
        acts_i[2] = -8'sd50;
        acts_i[3] =  8'sd0;

        // Row 0: +1,-1,+1,-1
        weights_i[0][0] = 1'b0; weights_i[0][1] = 1'b1;
        weights_i[0][2] = 1'b0; weights_i[0][3] = 1'b1;
        // Row 1: +1,-1,-1,+1
        weights_i[1][0] = 1'b0; weights_i[1][1] = 1'b1;
        weights_i[1][2] = 1'b1; weights_i[1][3] = 1'b0;
        // Row 2: +1,-1,+1,-1
        weights_i[2][0] = 1'b0; weights_i[2][1] = 1'b1;
        weights_i[2][2] = 1'b0; weights_i[2][3] = 1'b1;
        // Row 3: +1,-1,-1,+1
        weights_i[3][0] = 1'b0; weights_i[3][1] = 1'b1;
        weights_i[3][2] = 1'b1; weights_i[3][3] = 1'b0;

        @(posedge clk);
        #1;
        $display("Inputs: [%0d, %0d, %0d, %0d]",
                 acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: -51, Col1: 51, Col2: -305, Col3: 305");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n",
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        // ------------------------------------------------------------------
        // Test Case 3: All Zeros
        // ------------------------------------------------------------------
        $display("Test Case 3: All Zeros");

        acts_i[0] = 8'sd0;
        acts_i[1] = 8'sd0;
        acts_i[2] = 8'sd0;
        acts_i[3] = 8'sd0;

        weights_i[0] = 4'b0000;
        weights_i[1] = 4'b1111;
        weights_i[2] = 4'b0101;
        weights_i[3] = 4'b1010;

        @(posedge clk);
        #1;
        $display("Inputs: [%0d, %0d, %0d, %0d]",
                 acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: 0, Col1: 0, Col2: 0, Col3: 0");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n",
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        // ------------------------------------------------------------------
        // Test Case 4: Synchronous reset clears outputs mid-run
        // ------------------------------------------------------------------
        $display("Test Case 4: Synchronous reset clears outputs");

        acts_i[0] = 8'd5; acts_i[1] = 8'd5;
        acts_i[2] = 8'd5; acts_i[3] = 8'd5;
        weights_i = '0; // all +1 → col sums = 20

        @(posedge clk); // capture non-zero values
        #1;
        $display("Before reset — Actual Outputs: Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d",
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        rst = 1;
        @(posedge clk); // reset registered on this edge
        #1;
        $display("After  reset — Expected: 0,0,0,0  Actual: Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n",
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);
        rst = 0;

        $display("===============================================================");
        $display("   Simulation Complete");
        $display("===============================================================");

        $finish;
    end

endmodule
