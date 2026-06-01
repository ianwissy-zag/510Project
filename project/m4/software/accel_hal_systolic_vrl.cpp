// =============================================================================
// accel_hal_systolic_vrl.cpp — Streaming Verilator backend for HDL_Systolic_16x32.
//
// Streaming dataflow: weights loaded once per K-tile, then M_count activation
// rows streamed through the array in one continuous pass.  PE-row staggering
// (act_stagger module) keeps all 512 PEs active simultaneously → ~100% MAC
// utilization in steady state.
//
// Per K-tile call (hal_stream_tile):
//   1. Send ROWS weight beats via AXI (tuser = bank 0/1).
//   2. Assert start, wait ROWS cycles for LOAD_WT.
//   3. Stream M_count activation beats (one per cycle, tuser = 4).
//   4. Wait for done (ROWS+2 more cycles for pipeline drain).
//
// Readback (hal_read_results_all):
//   Streams 2×M_count AXI beats from the accumulator SRAM.
// =============================================================================

#include "accel_hal.h"
#include "Vsys_top.h"
#include "verilated.h"
#include <cstring>
#include <cstdio>

static VerilatedContext* g_ctx      = nullptr;
static Vsys_top*         g_dut      = nullptr;
static long long         g_fwd_tiles = 0;   // K-tiles (forward)
static long long         g_bwd_tiles = 0;   // K-tiles (backward)
static long long         g_hw_cycles_fwd = 0;
static long long         g_hw_cycles_bwd = 0;
static long long         g_tick_count    = 0;  // total simulated clock cycles
static int               g_wt_bank        = 0;  // alternates ping/pong each K-tile
static bool              g_wt_preloaded   = false; // current tile already in PEs via drain-overlap
static int               g_accum_bank          = 0;
static bool              g_accum_toggled       = false; // prevents double-toggle within an N-tile
static bool              g_accum_gelu_pending  = false; // biased+gelu pair in flight — block K-tile reset

// ── Async readback state ──────────────────────────────────────────────────────
// Non-null while a readback is in flight; beats are harvested inside tick() so
// any code that calls tick() (streaming, wait_done) implicitly drains the pipe.
static float* g_rb_buf   = nullptr;
static int    g_rb_need  = 0;   // 2 × M_count beats expected
static int    g_rb_beats = 0;   // beats collected so far
static int    g_rb_ab    = 0;   // saved apply_bias  (for warning)
static int    g_rb_ag    = 0;   // saved apply_gelu  (for warning)

// ── Clock / AXI helpers ───────────────────────────────────────────────────────
static void tick(void) {
    g_dut->clk = 0; g_dut->eval();
    // Harvest a beat PRE-posedge: this eval reflects the state left by the
    // previous posedge (running still 1, correct tdata).  Checking after the
    // posedge would miss the final beat because running goes 0 at that edge.
    if (g_rb_buf && g_rb_beats < g_rb_need
            && g_dut->m_axis_tvalid && g_dut->m_axis_tready) {
        int m = g_rb_beats / 2, half = g_rb_beats % 2;
        for (int i = 0; i < 16; i++) {
            uint32_t bits = g_dut->m_axis_tdata[i];
            memcpy(&g_rb_buf[m * ACCEL_SYS_COLS + half * 16 + i], &bits, sizeof(float));
        }
        g_rb_beats++;
    }
    g_dut->clk = 1; g_dut->eval();
    g_dut->clk = 0; g_dut->eval();
    g_tick_count++;
}

static void axi_idle(void) {
    g_dut->s_axis_tvalid = 0; g_dut->s_axis_tlast = 0;
    g_dut->s_axis_tuser  = 0;
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
}

// Send one 512-bit beat with 'n' uint16 values packed in the lower bits
static void send_beat(uint8_t tuser, bool tlast, const uint16_t* vals, int n) {
    memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
    for (int i = 0; i < n; i++)
        g_dut->s_axis_tdata[i/2] |= (uint32_t)vals[i] << ((i%2)*16);
    g_dut->s_axis_tuser  = tuser;
    g_dut->s_axis_tvalid = 1;
    g_dut->s_axis_tlast  = tlast ? 1 : 0;
    tick(); axi_idle();
}

// Send ROWS weight rows (one 512-bit beat per row, 32 BF16 each).
// The weight shift chain loads wt_in into PE[0] each cycle and shifts
// down, so the LAST beat sent ends up in PE[0] and the FIRST in PE[ROWS-1].
// Sending rows in REVERSE order (ROWS-1 down to 0) ensures PE[r] gets
// w_tile row r (K-element r), which pairs correctly with the stagger.
static void send_weight_tile(const bf16_t* w_tile, uint8_t tuser) {
    for (int r = ACCEL_SYS_ROWS - 1; r >= 0; r--) {
        send_beat(tuser, r == 0,
                  (const uint16_t*)(w_tile + r * ACCEL_SYS_COLS),
                  ACCEL_SYS_COLS);
    }
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
    g_dut->first_k_tile  = 1;
    g_dut->buf_sel       = 0;
    g_dut->preload_rdy   = 0;
    g_dut->accum_buf_sel = 0;
    g_dut->M_count       = 1;
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

// ── Streaming K-tile ─────────────────────────────────────────────────────────
// Loads weights then streams M_count activation rows.
// mode: 0=forward (tuser=0/1), 1=backward (tuser=2/3)
extern "C" void hal_stream_tile(const bf16_t* w_tile, const bf16_t* next_w_tile,
                                 const bf16_t* next_next_w_tile,
                                 const bf16_t* acts, int M_count, int first_k, int mode) {
    int bank = g_wt_bank;
    uint8_t wt_tuser = (uint8_t)bank;  // current bank = buf_sel for this tile

    // Set accumulator bank for this tile.  The toggle guard must NOT be reset while
    // a biased+gelu readback pair is in flight (g_accum_gelu_pending=true) because
    // the gelu half of the pair runs after these K-tiles and needs the same bank.
    if (!g_accum_gelu_pending)
        g_accum_toggled = false;
    g_dut->accum_buf_sel = (uint8_t)g_accum_bank;

    // Load current tile's weights into SRAM only if not already shifted into PEs
    // via a prior drain-overlap LOAD_WT.
    if (!g_wt_preloaded)
        send_weight_tile(w_tile, wt_tuser);

    // Trigger LOAD_WT → STREAM (or IDLE → STREAM directly if wt_preloaded_r is set).
    g_dut->mode         = mode;
    g_dut->first_k_tile = first_k ? 1 : 0;
    g_dut->buf_sel      = bank;
    g_dut->M_count      = (uint16_t)M_count;
    g_dut->preload_rdy  = 0;
    g_dut->start = 1; tick(); g_dut->start = 0;

    if (!g_wt_preloaded) {
        // During LOAD_WT (ROWS cycles): write K(n+1) into idle SRAM bank (bank^1),
        // then assert preload_rdy so the controller performs drain-overlap LOAD_WT.
        if (next_w_tile) {
            send_weight_tile(next_w_tile, (uint8_t)(bank ^ 1));
            g_dut->preload_rdy = 1;
        } else {
            for (int i = 0; i < ACCEL_SYS_ROWS; i++) tick();
        }
    } else {
        // K(n) already in PEs; K(n+1) already in SRAM bank ~buf_sel from the
        // previous tile's concurrent drain write.  Assert preload_rdy so the
        // controller performs drain-overlap LOAD_WT for K(n+1) during this drain.
        if (next_w_tile)
            g_dut->preload_rdy = 1;
    }

    // Stream M_count activation beats — one per cycle, tuser=100
    for (int m = 0; m < M_count; m++) {
        bool last = (m == M_count - 1);
        send_beat(4, last, (const uint16_t*)(acts + m * ACCEL_SYS_ROWS),
                  ACCEL_SYS_ROWS);
    }

    // Concurrent drain write: drain-overlap reads K(n+1) from bank ~buf_sel into PEs;
    // AXI simultaneously writes K(n+2) into bank buf_sel (wt_tuser=bank).
    // These are independent SRAM banks — no conflict.  ROWS beats fit in the
    // ROWS-cycle drain window so K(n+2) is ready before the next tile needs it.
    if (next_next_w_tile)
        send_weight_tile(next_next_w_tile, wt_tuser);

    if (!wait_done(M_count + ACCEL_SYS_ROWS + 10))
        fprintf(stderr, "[systolic/vrl] WARNING: done not asserted (stream)\n");
    g_dut->preload_rdy = 0;
    tick();

    // Both preloaded and non-preloaded paths assert preload_rdy when next_w_tile
    // is provided, so drain-overlap fires every K-tile → flag is simply non-null.
    g_wt_preloaded = (next_w_tile != nullptr);
    g_wt_bank ^= 1;  // alternate ping/pong for next K-tile
    if (mode == 0) g_fwd_tiles++;
    else           g_bwd_tiles++;
}

// ── Bias SRAM load ────────────────────────────────────────────────────────────
// Sends ACCEL_SYS_COLS FP32 bias values into the hardware bias SRAM via AXI
// (tuser=6, 3'b110).  Must be called before any readback that uses apply_bias=1.
// Two beats: beat 0 → bias[0..COLS/2-1], beat 1 → bias[COLS/2..COLS-1].
extern "C" void hal_load_bias(const float* bias, int N) {
    const int VPB = N / 2;  // FP32 values per beat
    for (int beat = 0; beat < 2; beat++) {
        memset(g_dut->s_axis_tdata, 0, sizeof(g_dut->s_axis_tdata));
        for (int i = 0; i < VPB; i++) {
            uint32_t bits;
            memcpy(&bits, &bias[beat * VPB + i], sizeof(float));
            g_dut->s_axis_tdata[i] = bits;
        }
        g_dut->s_axis_tuser  = 6;
        g_dut->s_axis_tvalid = 1;
        g_dut->s_axis_tlast  = (beat == 1) ? 1 : 0;
        tick(); axi_idle();
    }
}

// ── Readback helpers ──────────────────────────────────────────────────────────
// readback_start_internal: toggles accum bank (guarded), arms the hardware
//   readback unit, and sets g_rb_buf so tick() harvests beats automatically.
// readback_sync_internal:  drains any remaining beats and tears down state.
// Both halves are non-blocking relative to each other; between them the caller
// may stream K-tiles and beats will be collected inside every tick().

static void readback_start_internal(float* out, int M_count, int ab, int ag) {
    if (!g_accum_toggled) {
        g_accum_bank ^= 1;
        g_dut->accum_buf_sel = (uint8_t)g_accum_bank;
        g_accum_toggled = true;
    }
    g_rb_buf   = out;
    g_rb_need  = 2 * M_count;
    g_rb_beats = 0;
    g_rb_ab    = ab;
    g_rb_ag    = ag;
    g_dut->apply_bias    = ab;
    g_dut->apply_gelu    = ag;
    g_dut->m_axis_tready = 1;
    g_dut->rb_start = 1; tick(); g_dut->rb_start = 0;
}

static void readback_sync_internal(void) {
    if (!g_rb_buf) return;
    int max_t = (g_rb_need - g_rb_beats) * 8 + 40;
    for (int t = 0; t < max_t && g_rb_beats < g_rb_need; t++)
        tick();
    if (g_rb_beats < g_rb_need)
        fprintf(stderr, "[systolic/vrl] WARNING: only %d/%d readback beats "
                "(apply_bias=%d apply_gelu=%d)\n",
                g_rb_beats, g_rb_need, g_rb_ab, g_rb_ag);
    g_dut->apply_bias    = 0;
    g_dut->apply_gelu    = 0;
    g_dut->m_axis_tready = 0;
    g_rb_buf = nullptr;
    tick();
}

static void readback_all(float* out, int M_count, int ab, int ag) {
    readback_start_internal(out, M_count, ab, ag);
    readback_sync_internal();
}

// ── Readback public variants ──────────────────────────────────────────────────
// raw:              apply_bias=0, apply_gelu=0  (accumulator output only)
// gelu:             apply_bias=0, apply_gelu=1  (inference without bias)
// biased:           apply_bias=1, apply_gelu=0  (fch = matmul + bias)
// biased_gelu:      apply_bias=1, apply_gelu=1  (fch_gelu = GELU(matmul+bias))

extern "C" void hal_read_results_all(float* out, int M_count) {
    readback_all(out, M_count, 0, 0);
}
extern "C" void hal_read_results_all_gelu(float* out, int M_count) {
    readback_all(out, M_count, 0, 1);
}
extern "C" void hal_read_results_all_biased(float* out, int M_count) {
    readback_all(out, M_count, 1, 0);
}
extern "C" void hal_read_results_all_biased_gelu(float* out, int M_count) {
    readback_all(out, M_count, 1, 1);
}

// ── Async readback public API ─────────────────────────────────────────────────
// hal_readback_start / hal_readback_start_biased: arm hardware readback and
//   return immediately; beats are collected inside every subsequent tick().
// hal_readback_sync: drain remaining beats and release the readback unit.
// Only one readback may be in flight at a time.
extern "C" void hal_readback_start(float* out, int M_count) {
    readback_start_internal(out, M_count, 0, 0);
}
extern "C" void hal_readback_start_biased(float* out, int M_count) {
    readback_start_internal(out, M_count, 1, 0);
}
extern "C" void hal_readback_sync(void) {
    readback_sync_internal();
}

// hal_readback_start_biased_for_gelu: like hal_readback_start_biased but marks
//   a biased+gelu pair in flight so K-tile calls don't reset the toggle guard.
// hal_readback_done: call after BOTH readbacks of the pair complete to release
//   the guard and allow the next N-tile's readback to toggle the bank.
extern "C" void hal_readback_start_biased_for_gelu(float* out, int M_count) {
    readback_start_internal(out, M_count, 1, 0);
    g_accum_gelu_pending = true;
}
extern "C" void hal_readback_done(void) {
    g_accum_gelu_pending = false;
    g_accum_toggled      = false;
}

// ── Legacy single-tile interface (unused for systolic_vrl, kept for linking) ──
extern "C" void hal_compute_tile(const bf16_t*, const bf16_t*, int) {
    fprintf(stderr, "[systolic/vrl] hal_compute_tile: use hal_stream_tile\n");
}
extern "C" void hal_read_results(float*) {
    fprintf(stderr, "[systolic/vrl] hal_read_results: use hal_read_results_all\n");
}
extern "C" void hal_compute_tile_bwd(const bf16_t*, const bf16_t*, int) {
    fprintf(stderr, "[systolic/vrl] hal_compute_tile_bwd: use hal_stream_tile mode=1\n");
}

// ── Timing ────────────────────────────────────────────────────────────────────
extern "C" void accel_reset_timing(void) {
    g_fwd_tiles = 0; g_bwd_tiles = 0; g_wt_bank = 0; g_wt_preloaded = false;
    g_accum_bank = 0; g_accum_toggled = false; g_accum_gelu_pending = false; g_dut->accum_buf_sel = 0;
    g_hw_cycles_fwd = 0; g_hw_cycles_bwd = 0; g_tick_count = 0;
    g_rb_buf = nullptr; g_rb_need = 0; g_rb_beats = 0;
}

extern "C" long long hal_sim_cycle_snapshot(void) { return g_tick_count; }

extern "C" void hal_account_hw_cycles(long long cycles, int mode) {
    if (mode == 0) g_hw_cycles_fwd += cycles;
    else           g_hw_cycles_bwd += cycles;
}

extern "C" long long accel_get_hw_cycles_fwd(void) { return g_hw_cycles_fwd; }
extern "C" long long accel_get_hw_cycles_bwd(void) { return g_hw_cycles_bwd; }

extern "C" void accel_print_timing(void) {
    // Streaming: LOAD_WT=16 + STREAM=(M+ROWS+2) per K-tile.
    // In steady state (M >> ROWS), each K-tile ≈ M cycles.
    // Reported as M-tile-weighted average.
    double fwd_cycles = (double)g_fwd_tiles * ACCEL_SYS_ROWS;  // amortised
    double bwd_cycles = (double)g_bwd_tiles * ACCEL_SYS_ROWS;
    double wall_sec   = (fwd_cycles + bwd_cycles) / 595e6;
    printf("[accel/systolic_vrl] fwd_K-tiles=%lld bwd_K-tiles=%lld "
           "projected_hw_time=%.4f s  (@ ~595 MHz, streaming)\n",
           g_fwd_tiles, g_bwd_tiles, wall_sec);
}
