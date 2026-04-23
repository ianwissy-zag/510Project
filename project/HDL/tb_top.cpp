// Unified top-level testbench for accelerator_top.
//
// End-to-end flow tested:
//   1. Stream 32 weight rows over AXI-S (tuser=2'b00, ping buffer).
//      Each row = 32 × uint16(1) packed into 512-bit beat.
//   2. Stream 1 activation row over AXI-S (tuser=2'b10, ping buffer, tlast).
//      Row 0 = 32 × uint16(1).
//   3. Pulse start (act_buf_sel=0).
//   4. Poll until done.
//   5. Pulse rb_start; receive 64 AXI-S master beats (2 per output row).
//
// Expected result:
//   psum[c] = sum_{r=0}^{31} act_in[r] * W[r][c] = 32 * 1 * 1 = 32
//   for every column c in output_sram row 0.
//   Rows 1..31 of output_sram were never written so their value is 0.
//
// Simulation parameters (Makefile):
//   -GWT_DEPTH=64  -GWT_ADDR_WIDTH=6
#include "Vaccelerator_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static const int N          = 32;   // ARRAY_SIZE
static const int AXI_WORDS  = 16;   // 512 bits / 32 = 16 uint32_t words
static const int OUT_ROWS   = 32;   // output_sram depth
static int g_errors         = 0;

static uint16_t to_bf16(float f) {
    uint32_t bits; memcpy(&bits, &f, sizeof(bits)); return (uint16_t)(bits >> 16);
}
static uint32_t fp32_bits(float f) {
    uint32_t bits; memcpy(&bits, &f, sizeof(bits)); return bits;
}

static void chk(const char* lbl, uint32_t got, uint32_t exp) {
    if (got != exp) {
        printf("  FAIL [%s]: got %u  expected %u\n", lbl, got, exp);
        ++g_errors;
    }
}
static void chk_fp32(const char* lbl, uint32_t got, uint32_t exp) {
    if (got != exp) {
        float gf, ef; memcpy(&gf, &got, 4); memcpy(&ef, &exp, 4);
        printf("  FAIL [%s]: got 0x%08X (%.4f)  expected 0x%08X (%.4f)\n",
               lbl, got, gf, exp, ef);
        ++g_errors;
    }
}

static void tick(Vaccelerator_top* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

// Zero all AXI input signals.
static void axi_idle(Vaccelerator_top* dut) {
    dut->s_axis_tvalid = 0;
    dut->s_axis_tlast  = 0;
    dut->s_axis_tuser  = 0;
    memset(dut->s_axis_tdata, 0, sizeof(dut->s_axis_tdata));
}

// Send one 512-bit AXI beat.
//   tdata_u16: 32 uint16 values to pack into the 16 uint32 words.
static void axi_send_beat(Vaccelerator_top* dut,
                          uint8_t tuser, bool tlast,
                          uint16_t tdata_u16[N]) {
    // Pack 32 × uint16 into 16 × uint32 (little-endian within each word)
    for (int w = 0; w < AXI_WORDS; w++)
        dut->s_axis_tdata[w] = ((uint32_t)tdata_u16[2*w+1] << 16)
                              |  (uint32_t)tdata_u16[2*w];
    dut->s_axis_tuser  = tuser;
    dut->s_axis_tvalid = 1;
    dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(dut);
    axi_idle(dut);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vaccelerator_top* dut = new Vaccelerator_top{ctx};

    // ── Reset ─────────────────────────────────────────────────────────────
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

    // ── Build the beat payload: 32 × BF16(1.0) ──────────────────────────
    uint16_t ones[N];
    for (int i = 0; i < N; i++) ones[i] = to_bf16(1.0f);

    const uint32_t FP32_32 = fp32_bits(32.0f);   // 0x42000000
    const uint32_t FP32_64 = fp32_bits(64.0f);   // 0x42800000

    // ── Test 1: stream 32 weight rows (tuser=0 → weight ping) ────────────
    printf("Test 1: Write 32 weight rows via AXI-S\n");
    for (int row = 0; row < N; row++) {
        bool last = (row == N - 1);
        axi_send_beat(dut, /*tuser=*/0, last, ones);
    }
    // Allow a couple idle cycles for the SRAM to settle
    tick(dut); tick(dut);

    // ── Test 2: stream 1 activation row (tuser=2 → activation ping) ──────
    printf("Test 2: Write activation row 0 via AXI-S\n");
    axi_send_beat(dut, /*tuser=*/2, /*tlast=*/true, ones);
    tick(dut); tick(dut);

    // ── Test 3: start computation and wait for done ───────────────────────
    printf("Test 3: Pulse start, wait for done\n");
    dut->first_tile = 1; dut->last_tile = 1; dut->wt_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    bool saw_done = false;
    for (int t = 0; t < 200; t++) {
        tick(dut);
        if (dut->done) { saw_done = true; break; }
    }
    if (!saw_done) {
        printf("  FAIL: done never asserted within 200 cycles\n");
        ++g_errors;
    } else {
        printf("  done asserted\n");
    }
    tick(dut);  // absorb the done cycle before readback

    // ── Test 4: readback — verify psum[c]=32 in row 0 ────────────────────
    printf("Test 4: AXI readback — row 0 psum should be 32\n");
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(dut); dut->rb_start = 0;

    int beats_received = 0;
    bool saw_last      = false;
    char lbl[64];

    for (int t = 0; t < OUT_ROWS * 4 + 20; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            int row  = beats_received / 2;
            int beat = beats_received % 2;
            int base = beat * (N / 2);  // word index into psum (0 or 16)

            // Only validate row 0; rows 1..31 were never written
            if (row == 0) {
                for (int w = 0; w < AXI_WORDS; w++) {
                    snprintf(lbl, sizeof(lbl), "row0 beat%d word%d", beat, w);
                    chk_fp32(lbl, dut->m_axis_tdata[w], FP32_32);
                }
            }
            if (dut->m_axis_tlast) saw_last = true;
            ++beats_received;
        }
        tick(dut);
        if (!dut->rb_busy && beats_received > 0) break;
    }

    if (beats_received != OUT_ROWS * 2) {
        printf("  FAIL: received %d beats, expected %d\n",
               beats_received, OUT_ROWS * 2);
        ++g_errors;
    }
    if (!saw_last) {
        printf("  FAIL: m_axis_tlast never asserted\n");
        ++g_errors;
    }

    // ── Test 5: chip idle after readback ─────────────────────────────────
    printf("Test 5: Chip idle after readback\n");
    chk("rb_busy after done", (uint32_t)dut->rb_busy,    0);
    chk("done   after idle",  (uint32_t)dut->done,       0);
    chk("tvalid after idle",  (uint32_t)dut->m_axis_tvalid, 0);

    // ── Test 6: two-tile accumulation with ping-pong weight buffers ────────
    // Tile 1: weights=1 → bank 0, act=1 → ping, first_tile=1, last_tile=0
    // Tile 2: weights=1 → bank 1, act=1 → ping, first_tile=0, last_tile=1
    // Expected: psum[c] = 32 (tile 1) + 32 (tile 2) = 64
    printf("Test 6: Two-tile accumulation with ping-pong weights\n");

    // Load tile 1 weights into bank 0 (tuser=0)
    for (int row = 0; row < N; row++)
        axi_send_beat(dut, /*tuser=*/0, row == N-1, ones);
    tick(dut); tick(dut);

    // Load activation into act ping (tuser=2)
    axi_send_beat(dut, /*tuser=*/2, /*tlast=*/true, ones);
    tick(dut); tick(dut);

    // Start tile 1 — accumulate, do not write output yet
    dut->first_tile = 1; dut->last_tile = 0; dut->wt_buf_sel = 0;
    dut->act_buf_sel = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    // While tile 1 computes, stream tile 2 weights into bank 1 (tuser=1)
    for (int row = 0; row < N; row++)
        axi_send_beat(dut, /*tuser=*/1, row == N-1, ones);

    // Wait for tile 1 done
    for (int t = 0; t < 200; t++) { tick(dut); if (dut->done) break; }
    tick(dut);

    // Start tile 2 — accumulate from tile 1 and write output
    dut->first_tile = 0; dut->last_tile = 1; dut->wt_buf_sel = 1;
    dut->start = 1; tick(dut); dut->start = 0;

    for (int t = 0; t < 200; t++) { tick(dut); if (dut->done) break; }
    tick(dut);

    // Readback — expect psum[c] = 64
    dut->m_axis_tready = 1;
    dut->rb_start = 1; tick(dut); dut->rb_start = 0;

    int beats6 = 0;
    for (int t = 0; t < OUT_ROWS * 4 + 20; t++) {
        dut->eval();
        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            int row  = beats6 / 2;
            int beat = beats6 % 2;
            if (row == 0) {
                for (int w = 0; w < AXI_WORDS; w++) {
                    snprintf(lbl, sizeof(lbl), "t6 row0 beat%d word%d", beat, w);
                    chk_fp32(lbl, dut->m_axis_tdata[w], FP32_64);
                }
            }
            ++beats6;
        }
        tick(dut);
        if (!dut->rb_busy && beats6 > 0) break;
    }
    if (beats6 != OUT_ROWS * 2) {
        printf("  FAIL: received %d beats, expected %d\n", beats6, OUT_ROWS * 2);
        ++g_errors;
    }

    if (g_errors == 0) printf("All top-level tests PASSED.\n");
    else               printf("%d top-level test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
