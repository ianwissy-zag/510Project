// Testbench for the streaming systolic accelerator (sys_top).
//
// Test 1: M=4, single K-tile, W=1.0, act[m][k] = (m+1)
//   Expected: C[m][n] = ROWS*(m+1) for all n
//
// Test 2: M=4, two K-tiles, W=1.0, act=1.0
//   Expected: C[m][n] = 2*ROWS = 32 (K-tile accumulation)
//
// Test 3: M=16, W=2.0, act=1.0
//   Expected: C[m][n] = 2*ROWS = 32 for all m,n
//
// Test 4: backward mode, M=4, W^T=1.0, dout=1.0
//   Expected: C[m][n] = ROWS = 16 for all m,n

#include "Vsys_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static Vsys_top* dut;

static void tick() {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

static void axi_idle() {
    dut->s_axis_tvalid = 0; dut->s_axis_tlast = 0;
    dut->s_axis_tuser  = 0;
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
}

static uint16_t f2bf(float f) {
    uint32_t b; memcpy(&b, &f, 4); return (uint16_t)(b >> 16);
}

static void send_beat(uint8_t tuser, bool tlast, const uint16_t* vals, int n) {
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
    for (int i = 0; i < n; i++)
        dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(); axi_idle();
}

// Send ROWS weight rows (one 512-bit beat each, 32 BF16 per beat)
static void send_weights(const float* w_tile, uint8_t tuser) {
    const int ROWS = 32, COLS = 32;
    for (int r = 0; r < ROWS; r++) {
        uint16_t row[COLS];
        for (int c = 0; c < COLS; c++) row[c] = f2bf(w_tile[r*COLS + c]);
        send_beat(tuser, r == ROWS-1, row, COLS);
    }
    tick(); tick();
}

// Send one activation beat: ROWS BF16 values (one K-element per PE row)
static void send_act_beat(const float* act_row, bool last) {
    const int ROWS = 32;
    uint16_t vals[ROWS];
    for (int r = 0; r < ROWS; r++) vals[r] = f2bf(act_row[r]);
    send_beat(4, last, vals, ROWS);  // tuser=100
}

static bool wait_done(int max_cycles) {
    for (int t = 0; t < max_cycles; t++) {
        tick(); if (dut->done) return true;
    }
    return false;
}

// Read 2*M_count AXI beats from readback into out[M_count * COLS]
static void readback(float* out, int M_count) {
    const int COLS = 32, VPB = 16;  // values per beat
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(); dut->rb_start = 0;

    int beats = 0, need = 2 * M_count;
    for (int t = 0; t < need * 8 + 20 && beats < need; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            int m = beats / 2, half = beats % 2;
            for (int i = 0; i < VPB; i++) {
                int col = half * VPB + i;
                uint32_t bits = dut->m_axis_tdata[i];
                float val; memcpy(&val, &bits, 4);
                out[m * COLS + col] = val;
            }
            beats++;
        }
        tick();
    }
    dut->m_axis_tready = 0; tick();
}

// Load weights then run a full streaming K-tile
static void run_k_tile(const float* w_tile, const float* acts_flat,
                        int M_count, bool first_k, int wt_bank, int mode) {
    const int ROWS = 32;
    uint8_t wt_tuser = (mode == 0) ? (uint8_t)wt_bank : (uint8_t)(2 + wt_bank);

    send_weights(w_tile, wt_tuser);

    dut->mode         = mode;
    dut->first_k_tile = first_k ? 1 : 0;
    dut->fwd_buf_sel  = (mode == 0) ? wt_bank : 0;
    dut->bwd_buf_sel  = (mode == 1) ? wt_bank : 0;
    dut->M_count      = (uint16_t)M_count;
    dut->start = 1; tick(); dut->start = 0;

    // Wait for LOAD_WT to complete
    for (int i = 0; i < ROWS; i++) tick();

    // Stream M_count activation beats (one per cycle)
    for (int m = 0; m < M_count; m++)
        send_act_beat(acts_flat + m*ROWS, m == M_count-1);

    if (!wait_done(M_count + ROWS + 20))
        fprintf(stderr, "[tb] WARNING: done not asserted\n");
    tick();
}

static int chk(const char* label, float got, float exp, float tol = 0.5f) {
    if (fabsf(got - exp) > tol) {
        printf("  FAIL %s: got %.3f expected %.3f\n", label, got, exp);
        return 0;
    }
    return 1;
}

int main() {
    VerilatedContext ctx;
    dut = new Vsys_top{&ctx};

    dut->rst_n = 0; dut->start = 0; dut->mode = 0;
    dut->first_k_tile = 1; dut->fwd_buf_sel = 0; dut->bwd_buf_sel = 0;
    dut->M_count = 4; dut->rb_start = 0; dut->m_axis_tready = 0;
    axi_idle();
    for (int i = 0; i < 4; i++) tick();
    dut->rst_n = 1; dut->eval();

    const int ROWS = 32, COLS = 32;
    int all_pass = 1;
    float out[256 * COLS] = {};

    // ── Test 1 ────────────────────────────────────────────────────────────────
    // M=4, W=1.0, act[m][k] = (m+1)  →  C[m][n] = ROWS*(m+1)
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.0f;
        float acts[4*ROWS];
        for (int m=0;m<4;m++) for (int k=0;k<ROWS;k++) acts[m*ROWS+k]=(float)(m+1);

        run_k_tile(w, acts, 4, true, 0, 0);
        readback(out, 4);

        printf("Test 1: M=4, W=1.0, act[m][k]=m+1 → C[m][n]=ROWS*(m+1)\n");
        int ok=1;
        for (int m=0;m<4;m++) {
            char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(ROWS*(m+1)));
        }
        all_pass &= ok;
        printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 2 ────────────────────────────────────────────────────────────────
    // M=4, 2 K-tiles, W=1.0, act=1.0  →  C = 2*ROWS = 32
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.0f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.0f;

        run_k_tile(w, acts, 4, true,  0, 0);  // K-tile 0
        run_k_tile(w, acts, 4, false, 1, 0);  // K-tile 1 accumulates
        readback(out, 4);

        printf("Test 2: M=4, 2 K-tiles, W=1.0, act=1.0 → C=2*ROWS=64\n");
        int ok=1;
        for (int m=0;m<4;m++) {
            char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(2*ROWS));
        }
        all_pass &= ok;
        printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 3 ────────────────────────────────────────────────────────────────
    // M=16, W=2.0, act=1.0  →  C = 2*ROWS = 32
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=2.0f;
        float acts[16*ROWS]; for (int i=0;i<16*ROWS;i++) acts[i]=1.0f;

        run_k_tile(w, acts, 16, true, 0, 0);
        readback(out, 16);

        printf("Test 3: M=16, W=2.0, act=1.0 → C=2*ROWS=64\n");
        int ok=1;
        for (int m=0;m<16;m++) {
            char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)(2*ROWS));
        }
        all_pass &= ok;
        printf("  %s\n", ok?"PASS":"FAIL");
    }

    // ── Test 4 ────────────────────────────────────────────────────────────────
    // backward mode, M=4, W^T=1.0, dout=1.0  →  C = ROWS = 16
    {
        float w[ROWS*COLS]; for (int i=0;i<ROWS*COLS;i++) w[i]=1.0f;
        float acts[4*ROWS]; for (int i=0;i<4*ROWS;i++) acts[i]=1.0f;

        run_k_tile(w, acts, 4, true, 0, 1);  // mode=1 (backward)
        readback(out, 4);

        printf("Test 4: backward, M=4, W^T=1.0, dout=1.0 → C=ROWS=32\n");
        int ok=1;
        for (int m=0;m<4;m++) {
            char l[32]; snprintf(l,32,"C[%d][0]",m);
            ok &= chk(l, out[m*COLS], (float)ROWS);
        }
        all_pass &= ok;
        printf("  %s\n", ok?"PASS":"FAIL");
    }

    printf("\nAll streaming systolic tests %s.\n", all_pass?"PASSED":"FAILED");
    dut->final(); delete dut;
    return all_pass ? 0 : 1;
}
