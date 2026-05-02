// Testbench for sys_top (16×32 BF16 systolic accelerator).
//
// Test 1 (forward): W=BF16(1.0), act=BF16(1.0) → psum[j] = 16.0 (all cols)
// Test 2 (forward): W=BF16(2.0), act=BF16(1.0) → psum[j] = 32.0
// Test 3 (forward, 2-tile): W=BF16(1.0), act=BF16(1.0), first+last tile
//                            → psum[j] = 32.0  (two K=16 tiles, accumulate)
// Test 4 (backward dinp):    W^T=BF16(1.0), dout=BF16(1.0) → psum[j] = 16.0
//
// AXI protocol:
//   Weight: 16 × 512-bit beats (1 per row, 32 BF16 per beat)
//   Act:    1 × 512-bit beat   (16 BF16 in lower 256 bits)
//   Readback: 2 × 512-bit beats (32 FP32 = 1024 bits)

#include "Vsys_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static const int ROWS            = 16;
static const int COLS            = 32;
static const int WRD             = 16;  // uint32 words per AXI beat
static const int BEATS_PER_WT   = 16;  // AXI beats to write weight SRAM (one per row)
static const int BEATS_READBACK  = 2;   // 32 FP32 = 2 × 512-bit beats
static int g_errors = 0;

static uint16_t to_bf16(float f) {
    uint32_t b; memcpy(&b, &f, sizeof(b)); return (uint16_t)(b >> 16);
}
static uint32_t fp32_bits(float f) {
    uint32_t b; memcpy(&b, &f, sizeof(b)); return b;
}
static void chk(const char* lbl, uint32_t got, uint32_t exp) {
    if (got != exp) {
        float gf, ef; memcpy(&gf, &got, 4); memcpy(&ef, &exp, 4);
        printf("  FAIL [%s]: got 0x%08X (%.4f)  exp 0x%08X (%.4f)\n",
               lbl, got, gf, exp, ef);
        ++g_errors;
    }
}

static void tick(Vsys_top* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

static void axi_idle(Vsys_top* dut) {
    dut->s_axis_tvalid = 0; dut->s_axis_tlast = 0;
    dut->s_axis_tuser  = 0;
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
}

// Pack COLS=32 BF16 values into 16 uint32 words (2 per word)
static void send_wt_beat(Vsys_top* dut, uint8_t tuser, bool tlast,
                          uint16_t vals[32]) {
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
    for (int i = 0; i < 32; i++)
        dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(dut); axi_idle(dut);
}

// Send ROWS weight rows (one beat each)
static void send_weights(Vsys_top* dut, uint8_t tuser, uint16_t row_val[32]) {
    for (int r = 0; r < ROWS; r++)
        send_wt_beat(dut, tuser, r == ROWS-1, row_val);
    tick(dut); tick(dut);
}

// Pack ROWS=16 BF16 values into lower 256 bits of a 512-bit beat
static void send_activation(Vsys_top* dut, uint8_t tuser, uint16_t vals[16]) {
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
    for (int i = 0; i < 16; i++)
        dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = 1;
    tick(dut); axi_idle(dut);
    tick(dut); tick(dut);
}

static bool wait_done(Vsys_top* dut, int maxc) {
    for (int t = 0; t < maxc; t++) {
        tick(dut); if (dut->done) return true;
    }
    return false;
}

static void readback_check(Vsys_top* dut, uint32_t exp, const char* tag) {
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(dut); dut->rb_start = 0;
    int beats = 0; bool saw_last = false;
    char lbl[64];
    for (int t = 0; t < BEATS_READBACK*4 + 20; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            for (int w = 0; w < WRD; w++) {
                snprintf(lbl, sizeof(lbl), "%s b%d w%d", tag, beats, w);
                // Only first beat has valid COLS entries (beat0: cols 0..15, beat1: cols 16..31)
                chk(lbl, dut->m_axis_tdata[w], exp);
            }
            if (dut->m_axis_tlast) saw_last = true;
            ++beats;
        }
        tick(dut);
        if (beats >= BEATS_READBACK) break;
    }
    if (beats != BEATS_READBACK) {
        printf("  FAIL [%s]: got %d beats exp %d\n", tag, beats, BEATS_READBACK);
        ++g_errors;
    }
    if (!saw_last) { printf("  FAIL [%s]: no tlast\n", tag); ++g_errors; }
    dut->m_axis_tready = 0; tick(dut);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vsys_top* dut = new Vsys_top{ctx};

    // Reset
    dut->rst_n = 0; dut->start = 0; dut->mode = 0;
    dut->first_tile = 1; dut->last_tile = 1;
    dut->act_buf_sel = 0; dut->fwd_buf_sel = 0; dut->bwd_buf_sel = 0;
    dut->rb_start = 0; dut->m_axis_tready = 0;
    axi_idle(dut);
    for (int i = 0; i < 4; i++) tick(dut);
    dut->rst_n = 1; dut->eval();

    // BF16 payloads
    uint16_t ones_wt[32], twos_wt[32], ones_act[16];
    for (int i = 0; i < 32; i++) { ones_wt[i] = to_bf16(1.0f); twos_wt[i] = to_bf16(2.0f); }
    for (int i = 0; i < 16; i++) ones_act[i] = to_bf16(1.0f);

    // ── Test 1: forward, W=1.0, act=1.0 → 16.0 ───────────────────────────
    printf("Test 1: forward W=BF16(1.0) act=BF16(1.0) → FP32(16.0)\n");
    send_weights   (dut, 0x00, ones_wt);   // fwd weight ping
    send_activation(dut, 0x04, ones_act);  // act ping
    dut->mode=0; dut->first_tile=1; dut->last_tile=1;
    dut->fwd_buf_sel=0; dut->act_buf_sel=0;
    dut->start=1; tick(dut); dut->start=0;
    if (!wait_done(dut, 200)) { printf("  FAIL: no done\n"); ++g_errors; }
    else printf("  done\n");
    tick(dut);
    readback_check(dut, fp32_bits(16.0f), "t1");

    // ── Test 2: forward, W=2.0, act=1.0 → 32.0 ───────────────────────────
    printf("Test 2: forward W=BF16(2.0) act=BF16(1.0) → FP32(32.0)\n");
    send_weights   (dut, 0x00, twos_wt);
    send_activation(dut, 0x04, ones_act);
    dut->mode=0; dut->first_tile=1; dut->last_tile=1;
    dut->fwd_buf_sel=0; dut->act_buf_sel=0;
    dut->start=1; tick(dut); dut->start=0;
    if (!wait_done(dut, 200)) { printf("  FAIL: no done\n"); ++g_errors; }
    else printf("  done\n");
    tick(dut);
    readback_check(dut, fp32_bits(32.0f), "t2");

    // ── Test 3: forward 2-tile accumulation → 32.0 ────────────────────────
    // Tile 1: first_tile=1, last_tile=0 → accum = 16.0
    // Tile 2: first_tile=0, last_tile=1 → accum = 16.0+16.0 = 32.0
    printf("Test 3: 2-tile accumulation → FP32(32.0)\n");
    send_weights   (dut, 0x00, ones_wt);   // tile 1 into ping
    send_activation(dut, 0x04, ones_act);
    dut->mode=0; dut->first_tile=1; dut->last_tile=0;
    dut->fwd_buf_sel=0; dut->act_buf_sel=0;
    dut->start=1; tick(dut); dut->start=0;
    if (!wait_done(dut, 200)) { printf("  FAIL tile1\n"); ++g_errors; }
    else printf("  tile1 done\n");
    tick(dut);

    send_weights   (dut, 0x01, ones_wt);   // tile 2 into pong
    send_activation(dut, 0x04, ones_act);
    dut->mode=0; dut->first_tile=0; dut->last_tile=1;
    dut->fwd_buf_sel=1; dut->act_buf_sel=0;
    dut->start=1; tick(dut); dut->start=0;
    if (!wait_done(dut, 200)) { printf("  FAIL tile2\n"); ++g_errors; }
    else printf("  tile2 done\n");
    tick(dut);
    readback_check(dut, fp32_bits(32.0f), "t3");

    // ── Test 4: backward dinp, W^T=1.0, dout=1.0 → 16.0 ──────────────────
    // W^T is the same as W for all-1.0 (symmetric), so result is same as forward
    printf("Test 4: backward dinp W^T=BF16(1.0) dout=BF16(1.0) → FP32(16.0)\n");
    send_weights   (dut, 0x02, ones_wt);   // bwd weight ping (tuser=010)
    send_activation(dut, 0x04, ones_act);
    dut->mode=1; dut->first_tile=1; dut->last_tile=1;
    dut->bwd_buf_sel=0; dut->act_buf_sel=0;
    dut->start=1; tick(dut); dut->start=0;
    if (!wait_done(dut, 200)) { printf("  FAIL: no done\n"); ++g_errors; }
    else printf("  done\n");
    tick(dut);
    readback_check(dut, fp32_bits(16.0f), "t4");

    // Summary
    if (g_errors == 0) printf("All systolic_16x32 tests PASSED.\n");
    else               printf("%d systolic_16x32 test(s) FAILED.\n", g_errors);

    dut->final(); delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
