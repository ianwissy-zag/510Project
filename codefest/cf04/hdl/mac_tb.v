`timescale 1ns / 1ps

module tb_mac();

    // Testbench signals
    reg clk;
    reg rst;
    reg signed [7:0] a;
    reg signed [7:0] b;
    wire signed [31:0] out;
    
    // Variable to track how many errors occur
    integer error_count = 0;

    // Instantiate the Unit Under Test (UUT)
    mac uut (
        .clk(clk),
        .rst(rst),
        .a(a),
        .b(b),
        .out(out)
    );

    // Clock generation: 10ns period (5ns high, 5ns low)
    always #5 clk = ~clk;

    // Task to automatically check the output
    task check_output;
        input signed [31:0] expected_val;
        begin
            if (out !== expected_val) begin
                $display("ERROR at %0t: Expected %4d, but got %4d", $time, expected_val, out);
                error_count = error_count + 1;
            end else begin
                $display("PASS  at %0t: Output is correctly %4d", $time, out);
            end
        end
    endtask

    initial begin
        // 0. Initialize Inputs and perform a starting reset
        clk = 0;
        rst = 1; 
        a = 0;
        b = 0;
        
        // Wait for first negative edge, then check that reset worked
        @(negedge clk);
        check_output(0); 
        rst = 0; 
        
        // 1. Apply a=3, b=4 for three clock cycles
        a = 3;
        b = 4;
        @(negedge clk); check_output(12); // Cycle 1
        @(negedge clk); check_output(24); // Cycle 2
        @(negedge clk); check_output(36); // Cycle 3
        
        // 2. Assert rst for one clock cycle
        rst = 1;
        @(negedge clk); check_output(0);  // Cycle 4 (Reset drops it to 0)
        
        // 3. Assert a=-5 and b=2 for 2 cycles
        rst = 0;
        a = -5;
        b = 2;
        @(negedge clk); check_output(-10); // Cycle 5
        @(negedge clk); check_output(-20); // Cycle 6

        // Final report
        $display("--------------------------------------------------");
        if (error_count == 0) begin
            $display("SIMULATION PASSED! All outputs matched expected values.");
        end else begin
            $display("SIMULATION FAILED with %0d errors.", error_count);
        end
        $display("--------------------------------------------------");
        
        // End the simulation
        $finish;
    end

endmodule
