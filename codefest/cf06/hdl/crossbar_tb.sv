`timescale 1ns / 1ps

module tb_mac_crossbar_4x4;

    // Parameters
    localparam int INPUT_WIDTH = 8;
    localparam int OUTPUT_WIDTH = INPUT_WIDTH + 2;

    // Signals
    logic signed [INPUT_WIDTH-1:0] acts_i [3:0];
    logic [3:0][3:0] weights_i;
    logic signed [OUTPUT_WIDTH-1:0] mac_outs_o [3:0];

    // Instantiate the Device Under Test (DUT)
    crossbar_mac #(
        .INPUT_WIDTH(INPUT_WIDTH),
        .OUTPUT_WIDTH(OUTPUT_WIDTH)
    ) dut (
        .acts_i(acts_i),
        .weights_i(weights_i),
        .mac_outs_o(mac_outs_o)
    );

    initial begin
        // Optional: Dump waveforms for simulators like ModelSim/Vivado/Verilator
        $dumpfile("mac_tb.vcd");
        $dumpvars(0, tb_mac_crossbar_4x4);

        $display("===============================================================");
        $display("   4x4 MAC Crossbar Testbench");
        $display("   Weight Encoding: 0 = +1, 1 = -1");
        $display("===============================================================\n");

        // ----------------------------------------------------------------------
        // Test Case 1: User Requested Inputs
        // ----------------------------------------------------------------------
        $display("Test Case 1: User Request");
        
        // Input Vector: [10, 20, 30, 40]
        acts_i[0] = 8'd10;
        acts_i[1] = 8'd20;
        acts_i[2] = 8'd30;
        acts_i[3] = 8'd40;

        // Weight Matrix: [[ 1, -1,  1, -1],
        //                 [ 1,  1, -1, -1],
        //                 [-1,  1,  1, -1],
        //                 [-1, -1, -1,  1]]
        //
        // Note: weights_i[row][col] assignment (0 means +1, 1 means -1)
        
        // Row 0: 1, -1, 1, -1
        weights_i[0][0] = 1'b0; weights_i[0][1] = 1'b1; weights_i[0][2] = 1'b0; weights_i[0][3] = 1'b1;
        // Row 1: 1, 1, -1, -1
        weights_i[1][0] = 1'b0; weights_i[1][1] = 1'b0; weights_i[1][2] = 1'b1; weights_i[1][3] = 1'b1;
        // Row 2: -1, 1, 1, -1
        weights_i[2][0] = 1'b1; weights_i[2][1] = 1'b0; weights_i[2][2] = 1'b0; weights_i[2][3] = 1'b1;
        // Row 3: -1, -1, -1, 1
        weights_i[3][0] = 1'b1; weights_i[3][1] = 1'b1; weights_i[3][2] = 1'b1; weights_i[3][3] = 1'b0;

        #10; // Wait for combinational logic to settle

        // Print Results
        $display("Inputs: [%0d, %0d, %0d, %0d]", acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: -40, Col1: 0, Col2: -20, Col3: -20");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n", 
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        // ----------------------------------------------------------------------
        // Test Case 2: Negative Inputs / Extreme Values
        // ----------------------------------------------------------------------
        $display("Test Case 2: Negative Inputs / Extreme Values");
        
        // Input Vector: [-128, 127, -50, 0] (Testing 8-bit signed limits)
        acts_i[0] = -8'sd128; 
        acts_i[1] =  8'sd127; 
        acts_i[2] = -8'sd50;
        acts_i[3] =  8'sd0;

        // Row 0: +1, -1, +1, -1
        weights_i[0][0] = 1'b0; weights_i[0][1] = 1'b1; weights_i[0][2] = 1'b0; weights_i[0][3] = 1'b1;
        // Row 1: +1, -1, -1, +1
        weights_i[1][0] = 1'b0; weights_i[1][1] = 1'b1; weights_i[1][2] = 1'b1; weights_i[1][3] = 1'b0;
        // Row 2: +1, -1, +1, -1
        weights_i[2][0] = 1'b0; weights_i[2][1] = 1'b1; weights_i[2][2] = 1'b0; weights_i[2][3] = 1'b1;
        // Row 3: +1, -1, -1, +1
        weights_i[3][0] = 1'b0; weights_i[3][1] = 1'b1; weights_i[3][2] = 1'b1; weights_i[3][3] = 1'b0;

        #10;

        $display("Inputs: [%0d, %0d, %0d, %0d]", acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: -51, Col1: 51, Col2: -305, Col3: 305");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n", 
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        // ----------------------------------------------------------------------
        // Test Case 3: All Zeros
        // ----------------------------------------------------------------------
        $display("Test Case 3: All Zeros");
        
        acts_i[0] = 8'sd0;
        acts_i[1] = 8'sd0;
        acts_i[2] = 8'sd0;
        acts_i[3] = 8'sd0;

        // Weights can be random/mixed, output should remain exactly 0
        weights_i[0] = 4'b0000;
        weights_i[1] = 4'b1111;
        weights_i[2] = 4'b0101;
        weights_i[3] = 4'b1010;

        #10;

        $display("Inputs: [%0d, %0d, %0d, %0d]", acts_i[0], acts_i[1], acts_i[2], acts_i[3]);
        $display("Expected Outputs: Col0: 0, Col1: 0, Col2: 0, Col3: 0");
        $display("Actual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n", 
                 mac_outs_o[0], mac_outs_o[1], mac_outs_o[2], mac_outs_o[3]);

        $display("===============================================================");
        $display("   Simulation Complete");
        $display("===============================================================");
        
        $finish;
    end

endmodule
