// Testbench for the 32×32 systolic accelerator with bias + GELU post-processing.
//
// Tests 1-4:  streaming correctness (multi-row M, K-tile accumulation, backward mode)
// Test 5:     bias addition — matmul with W=1, act=1, bias=0.5 → C=ROWS+0.5
// Test 6:     GELU only — W=1, act=1, no bias → GELU(ROWS)
// Test 7:     bias + GELU together — bias=0.5, GELU applied after
// Test 8:     identity weight matrix — out[m][c]=c+1 (regression: weight reversal fix)
// Test 9:     M=1 minimum boundary
// Test 10:    M=64 stress test (4× previous largest M)
// Test 11:    negative activations — act=-1 → C=-ROWS (BF16 sign handling)
// Test 12:    per-column bias — bias[c]=c+1, verifies each column gets its own bias
// Test 13:    GELU on negative raw value — W=-1/32, act=1 → raw=-1, GELU(-1)≈-0.159
// Test 14:    M=256 (M_MAX boundary) — full accumulator depth
// Test 15:    4 K-tile accumulation — verifies multi-tile accumulation beyond 2 tiles
// Test 16:    ping-pong preloading — next tile loaded into idle SRAM bank during LOAD_WT
// Test 17:    drain-overlap LOAD_WT — next tile shifted into PEs during drain; LOAD_WT skipped
//
// NOTE: send_weights sends rows in REVERSE order (ROWS-1→0).  The weight shift
// chain loads wt_in into PE[0] each cycle and shifts down, so the last beat sent
// lands in PE[0] and the first in PE[ROWS-1].  Reversing the send order ensures
// PE[r] holds weight row r (K-element r), which is what the activation stagger
// delivers to row r.  Tests with uniform weights (1-7) are unaffected; Test 8
// specifically validates this mapping with a non-uniform (identity) weight matrix.

#include "Vsys_top.h"
#include "verilated.h"
#include "verilated_fst_c.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static Vsys_top*    dut;
static VerilatedFstC* tfp;
static int tb_accum_bank = 0;  // tracks which accumulator bank compute is using

static void tick() {
    dut->clk = 0; dut->eval(); dut->contextp()->timeInc(1); tfp->dump(dut->contextp()->time());
    dut->clk = 1; dut->eval(); dut->contextp()->timeInc(1); tfp->dump(dut->contextp()->time());
    dut->clk = 0; dut->eval(); dut->contextp()->timeInc(1); tfp->dump(dut->contextp()->time());
}

static void axi_idle() {
    dut->s_axis_tvalid = 0; dut->s_axis_tlast = 0;
    dut->s_axis_tuser  = 0;
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
}

static uint16_t f2bf(float f) { uint32_t b; memcpy(&b,&f,4); return (uint16_t)(b>>16); }

static void send_beat(uint8_t tuser, bool tlast, const uint16_t* vals, int n) {
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
    for (int i = 0; i < n; i++)
        dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(); axi_idle();
}

// Send ROWS weight rows (BF16 weights, tuser=bank).
// Sends rows in REVERSE order (r = ROWS-1 down to 0) so that after the
// top-to-bottom shift chain PE[r] holds w_tile row r (K-element r).
static void send_weights(const float* w_tile, uint8_t tuser) {
    const int ROWS = 32, COLS = 32;
    for (int r = ROWS - 1; r >= 0; r--) {
        uint16_t row[COLS];
        for (int c = 0; c < COLS; c++) row[c] = f2bf(w_tile[r*COLS+c]);
        send_beat(tuser, r == 0, row, COLS);
    }
    tick(); tick();
}

// Like send_weights but without the trailing idle ticks — exactly ROWS beats.
// Used for ping-pong preloading where the send must fit within LOAD_WT (ROWS cycles).
static void send_weights_nowait(const float* w_tile, uint8_t tuser) {
    const int ROWS = 32, COLS = 32;
    for (int r = ROWS - 1; r >= 0; r--) {
        uint16_t row[COLS];
        for (int c = 0; c < COLS; c++) row[c] = f2bf(w_tile[r*COLS+c]);
        send_beat(tuser, r == 0, row, COLS);
    }
}

// Send 32 FP32 bias values as 2 AXI beats (tuser=6 = 0b110)
static void send_bias(const float* bias) {
    const int COLS = 32, VPB = 16;
    for (int beat = 0; beat < 2; beat++) {
        memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
        for (int i = 0; i < VPB; i++) {
            uint32_t bits; memcpy(&bits, &bias[beat*VPB + i], 4);
            dut->s_axis_tdata[i] = bits;
        }
        dut->s_axis_tuser  = 6;  // 3'b110
        dut->s_axis_tvalid = 1;
        dut->s_axis_tlast  = (beat == 1) ? 1 : 0;
        tick(); axi_idle();
    }
}

static void send_act_beat(const float* act_row, bool last) {
    const int ROWS = 32;
    uint16_t vals[ROWS];
    for (int r = 0; r < ROWS; r++) vals[r] = f2bf(act_row[r]);
    send_beat(4, last, vals, ROWS);  // tuser=100
}

static bool wait_done(int max_cycles) {
    for (int t = 0; t < max_cycles; t++) { tick(); if (dut->done) return true; }
    return false;
}

// Read 2*M_count beats from readback into out[M_count * COLS]
static void readback(float* out, int M_count, bool apply_bias, bool apply_gelu) {
    const int COLS = 32, VPB = 16;
    // Toggle accum bank so hardware reads from the bank that was just written.
    // With accum_buf_sel=X: compute → bank X; readback → ~X bank.
    tb_accum_bank ^= 1;
    dut->accum_buf_sel = tb_accum_bank;
    dut->apply_bias    = apply_bias  ? 1 : 0;
    dut->apply_gelu    = apply_gelu  ? 1 : 0;
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(); dut->rb_start = 0;
    int beats = 0, need = 2 * M_count;
    for (int t = 0; t < need*8+40 && beats < need; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            int m = beats/2, half = beats%2;
            for (int i = 0; i < VPB; i++) {
                int col = half*VPB+i;
                uint32_t bits = dut->m_axis_tdata[i];
                memcpy(&out[m*COLS+col], &bits, sizeof(float));
            }
            beats++;
        }
        tick();
    }
    if (beats < need)
        fprintf(stderr, "  WARNING: only %d/%d readback beats\n", beats, need);
    dut->m_axis_tready = 0; tick();
}

static void run_k_tile(const float* w_tile, const float* acts_flat,
                        int M_count, bool first_k, int wt_bank, int mode) {
    const int ROWS = 32;
    send_weights(w_tile, (uint8_t)wt_bank);
    dut->accum_buf_sel = tb_accum_bank;
    dut->preload_rdy   = 0;
    dut->mode         = mode;
    dut->first_k_tile = first_k ? 1 : 0;
    dut->buf_sel      = wt_bank;
    dut->M_count      = (uint16_t)M_count;
    dut->start = 1; tick(); dut->start = 0;
    for (int i = 0; i < ROWS; i++) tick();
    for (int m = 0; m < M_count; m++)
        send_act_beat(acts_flat + m*ROWS, m == M_count-1);
    if (!wait_done(M_count + ROWS + 20))
        fprintf(stderr, "  WARNING: done not asserted\n");
    tick();
}

static int chk(const char* lbl, float got, float exp, float tol=0.5f) {
    if (fabsf(got-exp) > tol) {
        printf("  FAIL %s: got %.4f expected %.4f (err %.4f)\n",
               lbl, got, exp, fabsf(got-exp));
        return 0;
    }
    return 1;
}

// Reference GELU using exact tanhf for comparison
static float gelu_ref(float x) {
    float cube = 0.044715f * x*x*x;
    return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + cube)));
}

int main() {
    VerilatedContext ctx;
    ctx.traceEverOn(true);
    dut = new Vsys_top{&ctx};
    tfp = new VerilatedFstC;
    dut->trace(tfp, 99);
    tfp->open("waves.fst");

    dut->rst_n=0; dut->start=0; dut->mode=0;
    dut->first_k_tile=1; dut->buf_sel=0;
    dut->preload_rdy=0; dut->accum_buf_sel=0;
    dut->M_count=4; dut->rb_start=0; dut->m_axis_tready=0;
    dut->apply_bias=0; dut->apply_gelu=0;
    axi_idle();
    for (int i=0;i<4;i++) tick();
    dut->rst_n=1; dut->eval();

    const int ROWS=32, COLS=32;
    int all_pass = 1;
    float out[256*COLS] = {};

    // ── Test 1: M=4, W=1, act[m][k]=m+1 → C[m][n]=ROWS*(m+1) ───────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS];
        for (int m=0;m<4;m++) for (int k=0;k<ROWS;k++) acts[m*ROWS+k]=(float)(m+1);
        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4, false, false);
        printf("Test 1: M=4, W=1, act=m+1 → C=ROWS*(m+1)\n");
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(ROWS*(m+1))); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 2: M=4, 2 K-tiles → C=2*ROWS=64 ────────────────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        run_k_tile(w,acts,4,true,0,0); run_k_tile(w,acts,4,false,1,0);
        readback(out, 4, false, false);
        printf("Test 2: 2 K-tiles → C=2*ROWS=64\n");
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(2*ROWS)); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 3: M=16, W=2, act=1 → C=2*ROWS=64 ──────────────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=2.f;
        float acts[16*ROWS]; for (int i=0;i<16*ROWS;i++) acts[i]=1.f;
        run_k_tile(w,acts,16,true,0,0);
        readback(out, 16, false, false);
        printf("Test 3: M=16, W=2, act=1 → C=2*ROWS=64\n");
        int ok=1;
        for (int m=0;m<16;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(2*ROWS)); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 4: backward mode → C=ROWS=32 ────────────────────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        run_k_tile(w,acts,4,true,0,1);
        readback(out, 4, false, false);
        printf("Test 4: backward, W^T=1, dout=1 → C=ROWS=32\n");
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)ROWS); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 5: bias addition — W=1, act=1, bias=0.5 → C=ROWS+0.5 ───────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        float bias[COLS]; for (int c=0;c<COLS;c++) bias[c]=0.5f;
        send_bias(bias);
        run_k_tile(w,acts,4,true,0,0);
        readback(out, 4, true, false);
        float expected = (float)ROWS + 0.5f;
        printf("Test 5: bias=0.5 → C=ROWS+0.5=%.1f\n", expected);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], expected, 0.05f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 6: GELU only — W=1, act=1, no bias → GELU(ROWS) ────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        run_k_tile(w,acts,4,true,0,0);
        readback(out, 4, false, true);
        float expected = gelu_ref((float)ROWS);
        printf("Test 6: no bias, GELU(ROWS=32) → %.4f\n", expected);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], expected, 0.1f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 7: bias + GELU — W=1, act=1, bias=0.5 → GELU(ROWS+0.5) ─────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        float bias[COLS]; for (int c=0;c<COLS;c++) bias[c]=0.5f;
        send_bias(bias);
        run_k_tile(w,acts,4,true,0,0);
        readback(out, 4, true, true);
        float expected = gelu_ref((float)ROWS + 0.5f);
        printf("Test 7: bias=0.5, GELU(ROWS+0.5=32.5) → %.4f\n", expected);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], expected, 0.1f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 8: identity weight matrix → out[m][c] = c+1 ────────────────────
    // W[k][c] = (k==c) ? 1 : 0.  With act[m][k]=k+1: out[m][c] = c+1.
    // This test fails if PE[r] gets the wrong weight row (e.g., reversed order).
    {
        float w[ROWS*COLS] = {};
        for (int k = 0; k < ROWS; k++) w[k*COLS+k] = 1.0f;
        float acts[4*ROWS];
        for (int m=0;m<4;m++) for (int k=0;k<ROWS;k++) acts[m*ROWS+k]=(float)(k+1);
        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4, false, false);
        printf("Test 8: identity weights, act[m][k]=k+1 → out[m][c]=c+1\n");
        int ok=1;
        int cols[] = {0, 1, 15, 30, 31};
        for (int m=0;m<4;m++) {
            for (int ci=0;ci<5;ci++) {
                int c = cols[ci];
                char l[32]; snprintf(l,32,"C[%d][%d]",m,c);
                ok &= chk(l, out[m*COLS+c], (float)(c+1), 0.1f);
            }
        }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 9: M=1 minimum ──────────────────────────────────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[ROWS]; for (int i=0;i<ROWS;i++) acts[i]=1.f;
        run_k_tile(w, acts, 1, true, 0, 0);
        readback(out, 1, false, false);
        printf("Test 9: M=1 (minimum) → C=ROWS=%d\n", ROWS);
        int ok=1;
        for (int c=0;c<COLS;c++) { char l[32]; snprintf(l,32,"C[0][%d]",c);
            ok &= chk(l, out[c], (float)ROWS); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 10: M=64 stress ─────────────────────────────────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[64*ROWS]; for (int i=0;i<64*ROWS;i++) acts[i]=1.f;
        run_k_tile(w, acts, 64, true, 0, 0);
        readback(out, 64, false, false);
        printf("Test 10: M=64 stress → C=ROWS=%d for all rows\n", ROWS);
        int ok=1;
        int rows[] = {0, 1, 31, 32, 63};
        for (int ri=0;ri<5;ri++) { int m=rows[ri];
            char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)ROWS); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 11: negative activations — act=-1 → C=-ROWS ────────────────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=-1.f;
        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4, false, false);
        printf("Test 11: negative activations (act=-1) → C=-ROWS=%d\n", -ROWS);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(-ROWS)); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 12: per-column bias — bias[c]=c+1 → out[m][c]=ROWS+c+1 ─────────
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        float bias[COLS]; for (int c=0;c<COLS;c++) bias[c]=(float)(c+1);
        send_bias(bias);
        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4, true, false);
        printf("Test 12: per-column bias (bias[c]=c+1) → out[m][c]=ROWS+c+1\n");
        int ok=1;
        int cols[] = {0, 15, 31};
        for (int m=0;m<4;m++) {
            for (int ci=0;ci<3;ci++) {
                int c = cols[ci];
                char l[32]; snprintf(l,32,"C[%d][%d]",m,c);
                ok &= chk(l, out[m*COLS+c], (float)(ROWS+c+1), 0.5f);
            }
        }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 13: GELU on negative raw value ──────────────────────────────────
    // W=-1/ROWS, act=1 → raw = sum_k (1)*(-1/32) = -1.0 → GELU(-1) ≈ -0.159
    {
        float scale = -1.0f / (float)ROWS;
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=scale;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        float expected = gelu_ref(-1.0f);
        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4, false, true);
        printf("Test 13: GELU(-1.0) → expected %.4f\n", expected);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], expected, 0.05f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 14: M=256 (M_MAX boundary) ──────────────────────────────────────
    // Fills the accumulator SRAM to its maximum depth.  Checks first, last,
    // and boundary rows to verify the M_ADDR_W counter rolls over correctly.
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[256*ROWS]; for (int i=0;i<256*ROWS;i++) acts[i]=1.f;
        run_k_tile(w, acts, 256, true, 0, 0);
        readback(out, 256, false, false);
        printf("Test 14: M=256 (M_MAX) → C=ROWS=%d for all rows\n", ROWS);
        int ok=1;
        int rows14[] = {0, 1, 127, 128, 254, 255};
        for (int ri=0;ri<6;ri++) {
            int m=rows14[ri]; char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)ROWS);
        }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 15: 4 K-tile accumulation ───────────────────────────────────────
    // Accumulates across 4 K-tiles with alternating ping-pong banks.
    // Each tile contributes ROWS=32, so expected output = 4*ROWS=128.
    // Extends Test 2 (which covers only 2 tiles) to catch carry or overflow
    // bugs that emerge after the third and fourth accumulation steps.
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;
        run_k_tile(w, acts, 4, true,  0, 0);
        run_k_tile(w, acts, 4, false, 1, 0);
        run_k_tile(w, acts, 4, false, 0, 0);
        run_k_tile(w, acts, 4, false, 1, 0);
        readback(out, 4, false, false);
        printf("Test 15: 4 K-tiles → C=4*ROWS=%d\n", 4*ROWS);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(4*ROWS), 1.0f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 16: ping-pong preloading ────────────────────────────────────────
    // Verifies that writing next-tile weights into the idle bank during LOAD_WT
    // does not corrupt the active computation, and that the preloaded weights
    // are subsequently read correctly for the next K-tile.
    //
    // Tile 0: W=1 → bank 0; during its LOAD_WT, stream W=2 → bank 1 (preload).
    // Tile 1: weights already in bank 1 (no resend); buf_sel=1, first_k=false.
    // Expected: ROWS*1 + ROWS*2 = ROWS*3 = 96.
    {
        float w1[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w1[i]=1.f;
        float w2[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w2[i]=2.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;

        // Tile 0: send W=1 into bank 0, start, preload W=2 into bank 1 during LOAD_WT
        send_weights(w1, 0);
        dut->mode = 0; dut->first_k_tile = 1; dut->buf_sel = 0; dut->M_count = 4;
        dut->start = 1; tick(); dut->start = 0;
        send_weights_nowait(w2, 1);   // exactly ROWS beats, fits within LOAD_WT
        for (int m=0;m<4;m++) send_act_beat(acts + m*ROWS, m==3);
        if (!wait_done(4 + ROWS + 20)) fprintf(stderr, "  WARNING: done not asserted\n");
        tick();

        // Tile 1: bank 1 already holds W=2; skip weight send, let LOAD_WT read it
        dut->first_k_tile = 0; dut->buf_sel = 1; dut->M_count = 4;
        dut->start = 1; tick(); dut->start = 0;
        for (int i=0;i<ROWS;i++) tick();   // LOAD_WT reads preloaded W=2 from bank 1
        for (int m=0;m<4;m++) send_act_beat(acts + m*ROWS, m==3);
        if (!wait_done(4 + ROWS + 20)) fprintf(stderr, "  WARNING: done not asserted\n");
        tick();

        readback(out, 4, false, false);
        printf("Test 16: ping-pong preload (W=1 bank0 + preload W=2 bank1) → C=3*ROWS=%d\n", 3*ROWS);
        int ok=1;
        for (int m=0;m<4;m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(3*ROWS), 1.0f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 17: drain-overlap LOAD_WT ───────────────────────────────────────
    // Tile 0: W=1 → bank 0; preload W=2 → bank 1 during LOAD_WT; set preload_rdy=1.
    // The controller performs drain-overlap LOAD_WT from bank 1 during the drain,
    // setting wt_preloaded_r=1.  Tile 1 starts with start=1 and the hardware
    // transitions IDLE→STREAM directly (0-cycle LOAD_WT).  No LOAD_WT tick loop.
    // Expected output: ROWS*1 + ROWS*2 = 3*ROWS = 96.
    {
        float w1[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w1[i]=1.f;
        float w2[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w2[i]=2.f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.f;

        // Tile 0: send W=1 to SRAM bank 0, start LOAD_WT, preload W=2 into bank 1
        send_weights(w1, 0);
        dut->accum_buf_sel = tb_accum_bank;
        dut->mode=0; dut->first_k_tile=1; dut->buf_sel=0; dut->M_count=4;
        dut->preload_rdy=0;
        dut->start=1; tick(); dut->start=0;
        send_weights_nowait(w2, 1);   // preload bank 1 during LOAD_WT (exactly ROWS beats)
        dut->preload_rdy=1;           // signal controller: drain-overlap is safe
        for (int m=0; m<4; m++) send_act_beat(acts + m*ROWS, m==3);
        // Controller triggers drain-overlap LOAD_WT at stream_cnt=M_count+1=5,
        // shifting bank 1's weights into PEs over 32 cycles; sets wt_preloaded_r.
        if (!wait_done(4+ROWS+20)) fprintf(stderr, "  WARNING: done not asserted\n");
        dut->preload_rdy=0;
        tick();

        // Tile 1: hardware has wt_preloaded_r=1; start fires IDLE→STREAM (no LOAD_WT).
        dut->first_k_tile=0; dut->buf_sel=1; dut->M_count=4;
        dut->preload_rdy=0;
        dut->start=1; tick(); dut->start=0;
        // No LOAD_WT tick loop — hardware skips directly to STREAM.
        for (int m=0; m<4; m++) send_act_beat(acts + m*ROWS, m==3);
        if (!wait_done(4+ROWS+20)) fprintf(stderr, "  WARNING: done not asserted\n");
        tick();

        readback(out, 4, false, false);
        printf("Test 17: drain-overlap preload (skip LOAD_WT) → C=3*ROWS=%d\n", 3*ROWS);
        int ok=1;
        for (int m=0; m<4; m++) { char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(3*ROWS), 1.0f); }
        all_pass &= ok; printf("  %s\n", ok?"PASS":"FAIL");
    }

    printf("\nAll 32x32 tests %s  (%d/17)\n",
           all_pass ? "PASSED" : "FAILED",
           all_pass ? 17 : 0);
    tfp->close(); delete tfp;
    dut->final(); delete dut;
    return all_pass ? 0 : 1;
}
