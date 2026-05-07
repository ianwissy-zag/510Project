// =============================================================================
// accel_hal_systolic_vrl.cpp — Verilator backend for the 16×32 BF16 systolic.
//
// Drives sys_top (HDL_Systolic_16x32) via its AXI-S protocol.
//
// AXI tuser encoding:
//   3'b000 = forward weight ping   3'b001 = forward weight pong
//   3'b010 = backward weight ping  3'b011 = backward weight pong
//   3'b100 = activation ping       3'b101 = activation pong
//
// Weight write: 16 beats × 32 BF16 = 512 bits each (1 beat per row)
// Act write:    1 beat, 16 BF16 in lower 256 bits
// Readback:     2 × 512-bit beats (32 FP32 = 1024 bits)
//
// Forward pass:  mode=0, uses forward weight SRAM
// Backward dinp: mode=1, uses backward (transposed) weight SRAM
// =============================================================================

#include "accel_hal.h"
#include "Vsys_top.h"
#include "verilated.h"
#include <cstring>
#include <cstdio>

static VerilatedContext* g_ctx      = nullptr;
static Vsys_top*         g_dut      = nullptr;
static long long         g_fwd_tiles = 0;
static long long         g_bwd_tiles = 0;

// Bank selectors — ping-pong for pipelined double-buffering
static int  g_fwd_bank = 0;
static int  g_bwd_bank = 0;
static int  g_act_bank = 0;
static bool g_pending  = false;  // compute tile in flight, not yet waited on

// ── Clock / AXI helpers ───────────────────────────────────────────────────────
static void tick(void) {
    g_dut->clk = 0; g_dut->eval();
    g_dut->clk = 1; g_dut->eval();
    g_dut->clk = 0; g_dut->eval();
}

static void axi_idle(void) {
    g_dut->s_axis_tvalid = 0; g_dut->s_axis_tlast = 0;
    g_dut->s_axis_tuser  = 0;
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
}

// Pack ACCEL_SYS_COLS=32 BF16 values into 16 uint32 words (2 per word)
static void send_beat(uint8_t tuser, bool tlast, const uint16_t* vals, int n) {
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
    for (int i = 0; i < n; i++)
        g_dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    g_dut->s_axis_tuser  = tuser;
    g_dut->s_axis_tvalid = 1;
    g_dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(); axi_idle();
}

// Stream ACCEL_SYS_ROWS=16 weight rows (1 beat each, 32 BF16 per beat)
static void send_weight_tile(const bf16_t* w_tile, uint8_t tuser) {
    for (int r = 0; r < ACCEL_SYS_ROWS; r++) {
        send_beat(tuser, r == ACCEL_SYS_ROWS-1,
                  (const uint16_t*)(w_tile + r * ACCEL_SYS_COLS),
                  ACCEL_SYS_COLS);
    }
    tick(); tick();
}

// Stream 1 activation beat (ACCEL_SYS_ROWS=16 BF16 in lower 256 bits)
static void send_activation(const bf16_t* act, uint8_t tuser) {
    send_beat(tuser, true, (const uint16_t*)act, ACCEL_SYS_ROWS);
    tick(); tick();
}

static bool wait_done(int max_cycles) {
    for (int t = 0; t < max_cycles; t++) {
        tick(); if (g_dut->done) return true;
    }
    return false;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
extern "C" void accel_hal_init(void) {
    g_ctx = new VerilatedContext;
    g_dut = new Vsys_top{g_ctx};

    g_dut->rst_n         = 0;
    g_dut->start         = 0;
    g_dut->mode          = 0;
    g_dut->first_tile    = 1;
    g_dut->last_tile     = 1;
    g_dut->fwd_buf_sel   = 0;
    g_dut->bwd_buf_sel   = 0;
    g_dut->act_buf_sel   = 0;
    g_dut->rb_start      = 0;
    g_dut->m_axis_tready = 0;
    axi_idle();
    for (int i = 0; i < 4; i++) tick();
    g_dut->rst_n = 1; g_dut->eval();
}

extern "C" void accel_hal_free(void) {
    if (g_dut) { g_dut->final(); delete g_dut; g_dut = nullptr; }
    if (g_ctx) { delete g_ctx;  g_ctx = nullptr; }
}

// ── Forward tile ──────────────────────────────────────────────────────────────
// Pipeline: stream this tile's weights into the ALTERNATE buffer while the
// previous tile is still computing, then wait for done, then trigger this tile.
// Overlap: LOAD_WT (16 cy) coincides with COMPUTE (16 cy) of prior tile → ~100% util.
extern "C" void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act,
                                  int first_tile) {
    int next_fwd = 1 - g_fwd_bank;
    int next_act = 1 - g_act_bank;

    // Load into alternate buffers — safe to do while previous tile computes
    // because they use different SRAMs (ping vs pong).
    send_weight_tile(w_tile, (uint8_t)next_fwd);
    send_activation(act,     (uint8_t)(4 + next_act));

    // Now wait for the previous tile (if any) to finish before re-triggering.
    if (g_pending) {
        if (!wait_done(200))
            fprintf(stderr, "[systolic/vrl] WARNING: done not asserted (forward)\n");
        tick();
    }

    g_fwd_bank = next_fwd;
    g_act_bank = next_act;

    g_dut->mode        = 0;
    g_dut->first_tile  = first_tile ? 1 : 0;
    g_dut->last_tile   = 1;
    g_dut->fwd_buf_sel = g_fwd_bank;
    g_dut->act_buf_sel = g_act_bank;
    g_dut->start = 1; tick(); g_dut->start = 0;

    g_pending = true;
    g_fwd_tiles++;
}

// ── Backward dinp tile ────────────────────────────────────────────────────────
extern "C" void hal_compute_tile_bwd(const bf16_t* wT_tile, const bf16_t* dout,
                                      int first_tile) {
    int next_bwd = 1 - g_bwd_bank;
    int next_act = 1 - g_act_bank;

    send_weight_tile(wT_tile, (uint8_t)(2 + next_bwd));
    send_activation(dout,     (uint8_t)(4 + next_act));

    if (g_pending) {
        if (!wait_done(200))
            fprintf(stderr, "[systolic/vrl] WARNING: done not asserted (backward)\n");
        tick();
    }

    g_bwd_bank = next_bwd;
    g_act_bank = next_act;

    g_dut->mode        = 1;
    g_dut->first_tile  = first_tile ? 1 : 0;
    g_dut->last_tile   = 1;
    g_dut->bwd_buf_sel = g_bwd_bank;
    g_dut->act_buf_sel = g_act_bank;
    g_dut->start = 1; tick(); g_dut->start = 0;

    g_pending = true;
    g_bwd_tiles++;
}

// ── Readback ──────────────────────────────────────────────────────────────────
// 2 × 512-bit beats = 32 FP32 values = ACCEL_SYS_COLS results
extern "C" void hal_read_results(float* out) {
    static const int BEATS = 2;
    static const int WRD   = 16;

    // Drain the last in-flight tile before reading back results.
    if (g_pending) {
        if (!wait_done(200))
            fprintf(stderr, "[systolic/vrl] WARNING: done not asserted (readback drain)\n");
        tick();
        g_pending = false;
    }

    g_dut->m_axis_tready = 1;
    g_dut->rb_start = 1; tick(); g_dut->rb_start = 0;

    int beats = 0;
    for (int t = 0; t < BEATS * 4 + 20; t++) {
        g_dut->eval();
        if (g_dut->m_axis_tvalid && g_dut->m_axis_tready) {
            for (int w = 0; w < WRD; w++) {
                int idx = beats * WRD + w;
                if (idx < ACCEL_SYS_COLS) {
                    uint32_t bits = g_dut->m_axis_tdata[w];
                    memcpy(&out[idx], &bits, sizeof(float));
                }
            }
            ++beats;
        }
        tick();
        if (beats >= BEATS) break;
    }
    g_dut->m_axis_tready = 0; tick();
}

// ── Timing ────────────────────────────────────────────────────────────────────
extern "C" void accel_reset_timing(void) {
    g_fwd_tiles = 0; g_bwd_tiles = 0; g_pending = false;
}

extern "C" void accel_print_timing(void) {
    // Pipelined: steady-state = ROWS = 16 cycles per tile (LOAD_WT overlaps COMPUTE).
    // First tile pays an extra ROWS startup cycles, amortised over long matmuls.
    double fwd_cycles = (double)g_fwd_tiles * ACCEL_SYS_ROWS;
    double bwd_cycles = (double)g_bwd_tiles * ACCEL_SYS_ROWS;
    double wall_sec   = (fwd_cycles + bwd_cycles) / 595e6; // ~595 MHz
    printf("[accel/systolic_vrl] fwd_tiles=%lld bwd_tiles=%lld "
           "projected_hw_time=%.4f s  (@ ~595 MHz, pipelined)\n",
           g_fwd_tiles, g_bwd_tiles, wall_sec);
}
