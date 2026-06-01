// =============================================================================
// accel_hal_verilator.cpp — Cycle-accurate Verilator backend
//
// Implements the AXI-Stream protocol to drive vec_mac_top.
// The preprocessor symbol ACCEL_HDL_DIR must be set by the Makefile to the
// Verilator build output directory (obj_lib/) so that Vvec_mac_top.h resolves.
//
// AXI-S protocol summary:
//   Weights  : tuser=0(ping)/1(pong), 4 beats/row × K_DEPTH rows = 128 beats
//   Activations: tuser=2(ping)/3(pong), 1 beat (K_DEPTH BF16 values)
//   Readback : rb_start pulse → 8 × 512-bit beats on m_axis
//   Each beat : 512 bits = 16 uint32 words, 2 BF16 per word
// =============================================================================

#include "accel_hal.h"
#include "Vvec_mac_top.h"
#include "verilated.h"
#include <cstring>
#include <cstdio>
#include <cassert>

// ── Singleton DUT ─────────────────────────────────────────────────────────────
static VerilatedContext* g_ctx = nullptr;
static Vvec_mac_top*     g_dut = nullptr;
static long long         g_tile_count = 0;

// Ping-pong bank selectors — alternated each tile so the next weights can be
// streamed while the current computation runs (software here serialises them).
static int g_wt_bank  = 0;
static int g_act_bank = 0;

// ── Clock helpers ─────────────────────────────────────────────────────────────
static void tick(void) {
    g_dut->clk = 0; g_dut->eval();
    g_dut->clk = 1; g_dut->eval();
    g_dut->clk = 0; g_dut->eval();
}

static void axi_idle(void) {
    g_dut->s_axis_tvalid = 0;
    g_dut->s_axis_tlast  = 0;
    g_dut->s_axis_tuser  = 0;
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
}

// Pack 32 BF16 values into 16 uint32 words (two BF16 per word, little-endian).
static void pack_beat(const uint16_t* bf16_vals) {
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
    for (int i = 0; i < 32; i++) {
        int word  = i / 2;
        int shift = (i % 2) * 16;
        g_dut->s_axis_tdata[word] |= (uint32_t)bf16_vals[i] << shift;
    }
}

// Send one 512-bit AXI-S beat, then idle for one cycle.
static void send_beat(uint8_t tuser, bool tlast, const uint16_t* vals) {
    pack_beat(vals);
    g_dut->s_axis_tuser  = tuser;
    g_dut->s_axis_tvalid = 1;
    g_dut->s_axis_tlast  = tlast ? 1 : 0;
    tick();
    axi_idle();
}

// Wait for done signal (up to max_cycles).
static bool wait_done(int max_cycles) {
    for (int t = 0; t < max_cycles; t++) {
        tick();
        if (g_dut->done) return true;
    }
    return false;
}

// ── HAL implementation ────────────────────────────────────────────────────────

extern "C" void accel_hal_init(void) {
    g_ctx = new VerilatedContext;
    g_dut = new Vvec_mac_top{g_ctx};

    // Reset sequence
    g_dut->rst_n         = 0;
    g_dut->start         = 0;
    g_dut->act_buf_sel   = 0;
    g_dut->first_tile    = 1;
    g_dut->last_tile     = 1;
    g_dut->wt_buf_sel    = 0;
    g_dut->rb_start      = 0;
    g_dut->m_axis_tready = 0;
    axi_idle();
    for (int i = 0; i < 4; i++) tick();
    g_dut->rst_n = 1;
    g_dut->eval();
}

extern "C" void accel_hal_free(void) {
    if (g_dut) { g_dut->final(); delete g_dut; g_dut = nullptr; }
    if (g_ctx) { delete g_ctx; g_ctx = nullptr; }
}

extern "C" void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act,
                                  int first_tile) {
    // ── Stream weight tile ────────────────────────────────────────────────────
    // w_tile[k * VEC_SIZE + n]: K_DEPTH rows × VEC_SIZE cols
    // 4 AXI beats per row (32 BF16 per beat), K_DEPTH rows = 128 beats total
    int  total_wt_beats = ACCEL_K_DEPTH * 4;
    uint8_t wt_tuser    = (uint8_t)g_wt_bank;  // 0=ping, 1=pong

    for (int k = 0; k < ACCEL_K_DEPTH; k++) {
        for (int beat = 0; beat < 4; beat++) {
            uint16_t vals[32];
            for (int i = 0; i < 32; i++)
                vals[i] = w_tile[k * ACCEL_VEC_SIZE + beat * 32 + i];
            int beat_idx = k * 4 + beat;
            send_beat(wt_tuser, beat_idx == total_wt_beats - 1, vals);
        }
    }
    tick(); tick();

    // ── Stream activation vector ──────────────────────────────────────────────
    // 1 beat, K_DEPTH BF16 values packed into 32-element array (pad zeros)
    uint8_t  act_tuser = (uint8_t)(2 + g_act_bank);  // 2=ping, 3=pong
    uint16_t act_vals[32] = {0};
    for (int k = 0; k < ACCEL_K_DEPTH; k++) act_vals[k] = act[k];
    send_beat(act_tuser, true, act_vals);
    tick(); tick();

    // ── Trigger computation ───────────────────────────────────────────────────
    g_dut->first_tile  = first_tile ? 1 : 0;
    g_dut->last_tile   = 1;   // always write output (single-activation model)
    g_dut->wt_buf_sel  = g_wt_bank;
    g_dut->act_buf_sel = g_act_bank;
    g_dut->start = 1; tick(); g_dut->start = 0;

    if (!wait_done(200))
        fprintf(stderr, "[accel/verilator] WARNING: done never asserted\n");

    tick();
    g_tile_count++;
}

extern "C" void hal_read_results(float* out) {
    // Pulse rb_start, receive BEATS_READBACK=8 beats of FP32 psums
    static const int BEATS_READBACK = 8;
    static const int WRD            = 16;  // uint32 words per beat

    g_dut->m_axis_tready = 1;
    g_dut->rb_start = 1; tick(); g_dut->rb_start = 0;

    int beats = 0;
    for (int t = 0; t < BEATS_READBACK * 4 + 20; t++) {
        g_dut->eval();
        if (g_dut->m_axis_tvalid && g_dut->m_axis_tready) {
            for (int w = 0; w < WRD; w++) {
                int idx = beats * WRD + w;
                if (idx < ACCEL_VEC_SIZE) {
                    uint32_t bits = g_dut->m_axis_tdata[w];
                    memcpy(&out[idx], &bits, sizeof(float));
                }
            }
            beats++;
        }
        tick();
        if (!g_dut->rb_busy && beats > 0) break;
    }

    g_dut->m_axis_tready = 0;
    tick();
}

extern "C" void accel_reset_timing(void) { g_tile_count = 0; }

extern "C" void accel_print_timing(void) {
    double cycles   = (double)g_tile_count * ACCEL_K_DEPTH;
    double wall_sec = cycles / 606e6;  // 606 MHz from corrected synthesis (1650ps target)
    printf("[accel/verilator] tiles=%lld  simulated_cycles=%.0f"
           "  projected_hw_time=%.4f s\n",
           g_tile_count, cycles, wall_sec);
}
