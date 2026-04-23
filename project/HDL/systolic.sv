/* verilator lint_off DECLFILENAME */
module systolic_array_32x32 #(
    parameter ARRAY_SIZE = 32,
    parameter ACT_WIDTH  = 16,
    parameter WT_WIDTH   = 16,
    parameter PSUM_WIDTH = 32
)(
    input  logic clk,
    input  logic rst_n,
    
    input  logic load_wt, // Control signal to load weights
    
    // Edge inputs
    input  logic [ARRAY_SIZE-1:0][ACT_WIDTH-1:0]  act_in,   // Activations entering left edge
    input  logic [ARRAY_SIZE-1:0][WT_WIDTH-1:0]   wt_in,    // Weights entering top edge (during load)
    input  logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_in,  // Initial Psums entering top edge (usually 0)

    // Edge outputs
    output logic [ARRAY_SIZE-1:0][PSUM_WIDTH-1:0] psum_out  // Final accumulated results exiting bottom
);

    // ---------------------------------------------------------
    // Internal Wires for Grid Connections
    // ---------------------------------------------------------
    // Dimensions are sized +1 to account for the boundaries of the array
    
    // Horizontal activation wires [Row][Col]
    logic [ACT_WIDTH-1:0]  a_wires    [ARRAY_SIZE-1:0][ARRAY_SIZE:0];
    
    // Vertical weight shifting wires [Row][Col]
    logic [WT_WIDTH-1:0]   w_wires    [ARRAY_SIZE:0][ARRAY_SIZE-1:0];
    
    // Vertical partial sum wires [Row][Col]
    logic [PSUM_WIDTH-1:0] psum_wires [ARRAY_SIZE:0][ARRAY_SIZE-1:0];

    // ---------------------------------------------------------
    // Boundary Assignments
    // ---------------------------------------------------------
    genvar i;
    generate
        for (i = 0; i < ARRAY_SIZE; i++) begin : edges
            // Left edge receives incoming activations
            assign a_wires[i][0] = act_in[i];
            
            // Top edge receives incoming weights and initial partial sums
            assign w_wires[0][i]    = wt_in[i];
            assign psum_wires[0][i] = psum_in[i];
            
            // Bottom edge produces the final partial sums
            assign psum_out[i] = psum_wires[ARRAY_SIZE][i];
        end
    endgenerate

    // ---------------------------------------------------------
    // Generate the 32x32 Grid of Processing Elements
    // ---------------------------------------------------------
    genvar row, col;
    generate
        for (row = 0; row < ARRAY_SIZE; row++) begin : pe_rows
            for (col = 0; col < ARRAY_SIZE; col++) begin : pe_cols
                
                mac_pe #(
                    .ACT_WIDTH(ACT_WIDTH),
                    .WT_WIDTH(WT_WIDTH),
                    .PSUM_WIDTH(PSUM_WIDTH)
                ) pe_inst (
                    .clk(clk),
                    .rst_n(rst_n),
                    .load_wt(load_wt),
                    
                    // Inputs from top and left
                    .a_in(a_wires[row][col]),
                    .w_in(w_wires[row][col]),
                    .psum_in(psum_wires[row][col]),
                    
                    // Outputs to bottom and right
                    .a_out(a_wires[row][col+1]),
                    .w_out(w_wires[row+1][col]),
                    .psum_out(psum_wires[row+1][col])
                );
                
            end
        end
    endgenerate

endmodule
