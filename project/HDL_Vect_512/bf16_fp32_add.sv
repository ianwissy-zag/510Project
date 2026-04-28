`timescale 1ns / 1ps

// FP32 adder for BF16 MAC accumulation.
//
// Computes: result = a + b  (both FP32)
//
// Designed for use after bf16_mul — accumulates FP32 products into an FP32
// accumulator register.  Special case handling:
//   Denormal inputs   → flushed to zero (standard ML hardware behavior)
//   Infinity inputs   → infinity propagated
//   Exact cancellation → zero
//   Exponent overflow  → infinity
//   Exponent underflow → zero (flush-to-zero)
//
// Rounding: truncation (round-toward-zero).  Sufficient for training
// convergence; avoids guard/sticky bit complexity in a first implementation.

module bf16_fp32_add (
    input  logic [31:0] a,       // FP32 addend  (product from bf16_mul)
    input  logic [31:0] b,       // FP32 addend  (running accumulator)
    output logic [31:0] result   // FP32 sum
);

    // ── Field extraction ──────────────────────────────────────────────────
    logic        s_a, s_b;
    logic [7:0]  e_a, e_b;
    logic [22:0] m_a, m_b;

    assign s_a = a[31]; assign e_a = a[30:23]; assign m_a = a[22:0];
    assign s_b = b[31]; assign e_b = b[30:23]; assign m_b = b[22:0];

    // ── Special case flags ────────────────────────────────────────────────
    // Flush denormals (e==0) to zero along with true zeros.
    logic zero_a, zero_b, inf_a, inf_b;
    assign zero_a = (e_a == 8'h00);
    assign zero_b = (e_b == 8'h00);
    assign inf_a  = (e_a == 8'hFF);
    assign inf_b  = (e_b == 8'hFF);

    // ── Sort by magnitude so |M_L| >= |M_S| ──────────────────────────────
    // Compare {exponent, mantissa} to determine which operand is larger.
    // This ensures subtraction never produces a negative raw result.
    logic        do_swap;
    logic [7:0]  e_L, e_S;
    logic [22:0] m_L, m_S;
    logic        s_L, s_S;
    logic        z_L, z_S;

    assign do_swap = ({e_b, m_b} > {e_a, m_a});
    assign e_L = do_swap ? e_b : e_a;
    assign e_S = do_swap ? e_a : e_b;
    assign m_L = do_swap ? m_b : m_a;
    assign m_S = do_swap ? m_a : m_b;
    assign s_L = do_swap ? s_b : s_a;
    assign s_S = do_swap ? s_a : s_b;
    assign z_L = do_swap ? zero_b : zero_a;
    assign z_S = do_swap ? zero_a : zero_b;

    // ── Extended mantissas with implicit leading 1 ────────────────────────
    logic [23:0] M_L, M_S;
    assign M_L = z_L ? 24'h0 : {1'b1, m_L};
    assign M_S = z_S ? 24'h0 : {1'b1, m_S};

    // ── Align M_S to the scale of M_L ────────────────────────────────────
    logic [7:0]  sh;
    logic [23:0] M_S_al;
    assign sh     = e_L - e_S;
    assign M_S_al = (sh >= 8'd24) ? 24'h0 : (M_S >> sh);

    // ── Add or subtract ───────────────────────────────────────────────────
    logic        do_sub;
    logic [24:0] raw;   // 25 bits: bit 24 catches addition overflow

    assign do_sub = s_L ^ s_S;
    assign raw    = do_sub ? ({1'b0, M_L} - {1'b0, M_S_al})
                           : ({1'b0, M_L} + {1'b0, M_S_al});

    // ── Leading zero count for normalization after subtraction ────────────
    // Finds the position of the highest set bit in raw[23:0].
    // lzc = number of leading zeros = 23 - position_of_msb
    logic [4:0] lzc;
    always_comb begin
        lzc = 5'd24;  // default: raw[23:0] is all zeros
        for (int i = 0; i <= 23; i++)   // LSB-to-MSB: higher bits win
            if (raw[i]) lzc = 5'd23 - 5'(unsigned'(i));
    end

    // ── Normalize ─────────────────────────────────────────────────────────
    logic [8:0]  exp_r;    // 9 bits to detect overflow/underflow
    logic [22:0] mant_r;
    logic [23:0] lshift;   // left-shifted mantissa for subtraction normalization
    assign lshift = raw[23:0] << lzc;

    always_comb begin
        exp_r  = 9'd0;
        mant_r = 23'h0;
        if (raw[24]) begin
            // Addition overflowed into bit 24 — shift right 1, exp++
            mant_r = raw[23:1];
            exp_r  = {1'b0, e_L} + 9'd1;
        end else if (raw[23]) begin
            // Already normalized — bit 23 is the implicit leading 1
            mant_r = raw[22:0];
            exp_r  = {1'b0, e_L};
        end else begin
            // Subtraction cancelled leading bits — shift left by lzc
            mant_r = lshift[22:0];
            exp_r  = ({1'b0, e_L} > {4'b0, lzc}) ?
                     ({1'b0, e_L} - {4'b0, lzc}) : 9'd0;
        end
    end

    // ── Output mux ────────────────────────────────────────────────────────
    always_comb begin
        if (inf_a || inf_b) begin
            // Propagate infinity — use sign of whichever triggered it
            result = inf_a ? {s_a, 8'hFF, 23'h0} : {s_b, 8'hFF, 23'h0};
        end else if (zero_a && zero_b) begin
            result = 32'h0;
        end else if (zero_a) begin
            result = b;          // a flushed to zero — return b unchanged
        end else if (zero_b) begin
            result = a;          // b flushed to zero — return a unchanged
        end else if (~|raw) begin
            result = 32'h0;      // exact cancellation
        end else if (exp_r == 9'd0 || exp_r[8]) begin
            result = {s_L, 31'h0};  // underflow → zero
        end else if (exp_r >= 9'd255) begin
            result = {s_L, 8'hFF, 23'h0};  // overflow → infinity
        end else begin
            result = {s_L, exp_r[7:0], mant_r};
        end
    end

endmodule
