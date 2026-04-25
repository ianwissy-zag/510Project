// End-to-end BF16 testbench for vec_mac_top.
//
// All weights and activations are BF16; accumulators and outputs are FP32.
// BF16 is the upper 16 bits of FP32, so conversion is a single shift.
//
// Test 1: all weights = BF16(1.0), all acts = BF16(1.0)
//   psum[j] = 32 × 1.0 × 1.0 = FP32(32.0) = 0x42000000
//
// Test 2: all weights = BF16(1.0), acts = BF16(1.0, 2.0, ... 32.0)
//   psum[j] = 1+2+...+32 = FP32(528.0) = 0x44040000
//
// Test 3: two-tile accumulation (weights=1.0, acts=1.0 both tiles)
//   psum[j] = 32.0 + 32.0 = FP32(64.0) = 0x42800000
//
// AXI protocol:
//   Weight: 4 × 512-bit beats per SRAM row (32 BF16 elements per beat)
//           32 rows → 128 beats total per weight matrix
//   Act:    1 × 512-bit beat per ping/pong slot (32 BF16 elements)
//   Readback: 8 × 512-bit beats (128 × FP32 = 4096 bits)

#include "Vvec_mac_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static const int VEC  = 128; // VEC_SIZE
static const int K    = 32;  // K_DEPTH
static const int WRD  = 16;  // AXI words per beat (512 / 32)
static const int BEATS_PER_WT_ROW = 4;
static const int BEATS_READBACK   = 8;  // 4096 / 512
static int g_errors = 0;

// Convert a float to its BF16 bit pattern (upper 16 bits of FP32).
static uint16_t to_bf16(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return (uint16_t)(bits >> 16);
}

static void chk_fp32(const char* lbl, uint32_t got, uint32_t exp) {
    if (got != exp) {
        float got_f, exp_f;
        memcpy(&got_f, &got, 4);
        memcpy(&exp_f, &exp, 4);
        printf("  FAIL [%s]: got 0x%08X (%.4f)  expected 0x%08X (%.4f)\n",
               lbl, got, got_f, exp, exp_f);
        ++g_errors;
    }
}

static void tick(Vvec_mac_top* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

static void axi_idle(Vvec_mac_top* dut) {
    dut->s_axis_tvalid = 0;
    dut->s_axis_tlast  = 0;
    dut->s_axis_tuser  = 0;
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
}

// Pack 32 BF16 values into the 512-bit (16 × uint32) AXI data word.
// Each uint32 holds two consecutive BF16 values (little-endian).
static void pack_bf16(Vvec_mac_top* dut, const uint16_t bf16_vals[32]) {
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
    for (int i = 0; i < 32; i++) {
        int word  = i / 2;
        int shift = (i % 2) * 16;
        dut->s_axis_tdata[word] |= (uint32_t)bf16_vals[i] << shift;
    }
}

static void send_beat(Vvec_mac_top* dut, uint8_t tuser, bool tlast,
                      const uint16_t bf16_vals[32]) {
    pack_bf16(dut, bf16_vals);
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(dut);
    axi_idle(dut);
}

// Stream a full weight matrix: K_DEPTH rows × 4 beats each = 128 beats.
// Each beat carries 32 BF16 elements (one quarter of a 128-element row).
// For a uniform-weight matrix, the same 32-element beat is repeated.
static void send_weights(Vvec_mac_top* dut, uint8_t tuser,
                         const uint16_t beat_vals[32]) {
    int total = K * BEATS_PER_WT_ROW; // 128 beats
    for (int b = 0; b < total; b++)
        send_beat(dut, tuser, b == total - 1, beat_vals);
    tick(dut); tick(dut);
}

// Stream one 512-bit activation vector beat (32 BF16 scalars).
static void send_activations(Vvec_mac_top* dut, uint8_t tuser,
                              const uint16_t bf16_vals[32]) {
    send_beat(dut, tuser, true, bf16_vals);
    tick(dut); tick(dut);
}

static bool wait_done(Vvec_mac_top* dut, int max_cycles) {
    for (int t = 0; t < max_cycles; t++) {
        tick(dut);
        if (dut->done) return true;
    }
    return false;
}

// Stream BEATS_READBACK beats and verify each FP32 psum matches exp_bits.
static void readback_and_check(Vvec_mac_top* dut, uint32_t exp_fp32_bits,
                               const char* tag) {
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(dut); dut->rb_start = 0;

    int  beats    = 0;
    bool saw_last = false;
    char lbl[128];

    for (int t = 0; t < BEATS_READBACK * 4 + 20; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            for (int w = 0; w < WRD; w++) {
                snprintf(lbl, sizeof(lbl), "%s beat%d word%d", tag, beats, w);
                chk_fp32(lbl, dut->m_axis_tdata[w], exp_fp32_bits);
            }
            if (dut->m_axis_tlast) saw_last = true;
            beats++;
        }
        tick(dut);
        if (!dut->rb_busy && beats > 0) break;
    }

    if (beats != BEATS_READBACK) {
        printf("  FAIL [%s]: received %d beats, expected %d\n",
               tag, beats, BEATS_READBACK);
        ++g_errors;
    }
    if (!saw_last) {
        printf("  FAIL [%s]: m_axis_tlast never seen\n", tag);
        ++g_errors;
    }
    dut->m_axis_tready = 0;
    tick(dut);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vvec_mac_top* dut = new Vvec_mac_top{ctx};

    // ── Reset ──────────────────────────────────────────────────────────────
    dut->rst_n         = 0;
    dut->start         = 0;
    dut->act_buf_sel   = 0;
    dut->first_tile    = 1;
    dut->last_tile     = 1;
    dut->wt_buf_sel    = 0;
    dut->rb_start      = 0;
    dut->m_axis_tready = 0;
    axi_idle(dut);
    for (int i = 0; i < 4; i++) tick(dut);
    dut->rst_n = 1; dut->eval();

    // ── BF16 payload patterns ─────────────────────────────────────────────
    // All-ones pattern: 32 × BF16(1.0) = 32 × 0x3F80
    uint16_t ones_bf16[32];
    for (int i = 0; i < 32; i++) ones_bf16[i] = to_bf16(1.0f);

    // Ramp pattern: BF16(1.0, 2.0, ..., 32.0)
    uint16_t ramp_bf16[32];
    for (int i = 0; i < 32; i++) ramp_bf16[i] = to_bf16((float)(i + 1));

    // Expected FP32 output bit patterns
    const uint32_t fp32_32  = 0x42000000u; // 32.0
    const uint32_t fp32_528 = 0x44040000u; // 528.0
    const uint32_t fp32_64  = 0x42800000u; // 64.0

    printf("BF16(1.0) = 0x%04X\n", ones_bf16[0]);
    printf("BF16(32.0)= 0x%04X\n", ramp_bf16[31]);

    // ══════════════════════════════════════════════════════════════════════
    // Test 1: BF16(1.0) weights × BF16(1.0) acts → FP32(32.0)
    // ══════════════════════════════════════════════════════════════════════
    printf("Test 1: all BF16(1.0) weights and activations (expect FP32 32.0 = 0x%08X)\n",
           fp32_32);
    send_weights    (dut, 0, ones_bf16);
    send_activations(dut, 2, ones_bf16);

    dut->first_tile = 1; dut->last_tile = 1;
    dut->wt_buf_sel = 0; dut->act_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    if (!wait_done(dut, 200)) {
        printf("  FAIL: done never asserted\n"); ++g_errors;
    } else printf("  done asserted\n");
    tick(dut);

    readback_and_check(dut, fp32_32, "t1");

    // ══════════════════════════════════════════════════════════════════════
    // Test 2: BF16(1.0) weights × BF16(1..32) acts → FP32(528.0)
    //   psum[j] = sum_{k=0}^{31} 1.0 * (k+1) = 1+2+...+32 = 528
    // ══════════════════════════════════════════════════════════════════════
    printf("Test 2: BF16(1.0) weights, BF16(1..32) acts (expect FP32 528.0 = 0x%08X)\n",
           fp32_528);
    send_weights    (dut, 0, ones_bf16);
    send_activations(dut, 2, ramp_bf16);

    dut->first_tile = 1; dut->last_tile = 1;
    dut->wt_buf_sel = 0; dut->act_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    if (!wait_done(dut, 200)) {
        printf("  FAIL: done never asserted\n"); ++g_errors;
    } else printf("  done asserted\n");
    tick(dut);

    readback_and_check(dut, fp32_528, "t2");

    // ══════════════════════════════════════════════════════════════════════
    // Test 3: two-tile accumulation → FP32(64.0)
    //   Tile 1: W ping, act ping, first_tile=1, last_tile=0 → acc = 32.0
    //   Tile 2: W pong, act ping, first_tile=0, last_tile=1 → acc = 64.0
    // ══════════════════════════════════════════════════════════════════════
    printf("Test 3: two-tile BF16 accumulation (expect FP32 64.0 = 0x%08X)\n",
           fp32_64);

    send_weights    (dut, 0, ones_bf16);   // tile 1 weights → ping bank
    send_activations(dut, 2, ones_bf16);   // acts → ping

    dut->first_tile = 1; dut->last_tile = 0;
    dut->wt_buf_sel = 0; dut->act_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    if (!wait_done(dut, 200)) {
        printf("  FAIL: done (tile1) never asserted\n"); ++g_errors;
    } else printf("  tile1 done\n");
    tick(dut);

    send_weights(dut, 1, ones_bf16);       // tile 2 weights → pong bank

    dut->first_tile = 0; dut->last_tile = 1;
    dut->wt_buf_sel = 1; dut->act_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    if (!wait_done(dut, 200)) {
        printf("  FAIL: done (tile2) never asserted\n"); ++g_errors;
    } else printf("  tile2 done\n");
    tick(dut);

    readback_and_check(dut, fp32_64, "t3");

    // ── Summary ────────────────────────────────────────────────────────────
    if (g_errors == 0) printf("All BF16 vec_mac_top tests PASSED.\n");
    else               printf("%d BF16 vec_mac_top test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
