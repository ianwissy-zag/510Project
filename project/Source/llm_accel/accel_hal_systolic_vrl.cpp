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
static int               g_accum_bank     = 0;  // accumulator ping/pong: toggles at each readback

// ── Clock / AXI helpers ───────────────────────────────────────────────────────
static void tick(void) {
    g_dut->clk = 0; g_dut->eval();
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
                                 const bf16_t* acts, int M_count, int first_k, int mode) {
    int bank = g_wt_bank;
    uint8_t wt_tuser = (uint8_t)bank;

    // Set accumulator bank for this tile (constant across all K-tiles of one N-tile).
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
        // During LOAD_WT (ROWS cycles): preload next tile's weights into the idle
        // SRAM bank, then signal the controller to perform drain-overlap LOAD_WT.
        if (next_w_tile) {
            int next_bank = bank ^ 1;
            send_weight_tile(next_w_tile, (uint8_t)next_bank);
            g_dut->preload_rdy = 1;  // next tile is in SRAM; controller will drain-overlap
        } else {
            for (int i = 0; i < ACCEL_SYS_ROWS; i++) tick();
        }
    }
    // If wt_preloaded (LOAD_WT skipped by hardware): preload_rdy was already set;
    // weights for the next tile were written to SRAM during the previous drain window.
    g_wt_preloaded = false;

    // Stream M_count activation beats — one per cycle, tuser=100
    for (int m = 0; m < M_count; m++) {
        bool last = (m == M_count - 1);
        send_beat(4, last, (const uint16_t*)(acts + m * ACCEL_SYS_ROWS),
                  ACCEL_SYS_ROWS);
    }

    // Wait for drain to complete.  The controller triggers drain-overlap LOAD_WT
    // at stream_cnt=M_count+1 (if preload_rdy=1), finishing one cycle before DONE.
    if (!wait_done(M_count + ACCEL_SYS_ROWS + 10))
        fprintf(stderr, "[systolic/vrl] WARNING: done not asserted (stream)\n");
    g_dut->preload_rdy = 0;
    tick();

    // If drain-overlap ran, next tile's weights are now in PEs (wt_preloaded_r set
    // in hardware); record this so the next call skips the SRAM→PE LOAD_WT phase.
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

// ── Readback (internal helper) ────────────────────────────────────────────────
// Triggers one readback with the requested apply_bias / apply_gelu flags.
// accum_sram is read-only during readback so multiple calls from the same
// accumulated result are safe.
static void readback_all(float* out, int M_count, int ab, int ag) {
    const int VPB  = 16;
    const int COLS = ACCEL_SYS_COLS;

    // Toggle accum_buf_sel so readback reads from the bank that was just written.
    // After toggle: hardware reads from ~accum_buf_sel = the previously-written bank.
    g_accum_bank ^= 1;
    g_dut->accum_buf_sel = (uint8_t)g_accum_bank;

    g_dut->apply_bias    = ab;
    g_dut->apply_gelu    = ag;
    g_dut->m_axis_tready = 1;
    g_dut->rb_start = 1; tick(); g_dut->rb_start = 0;

    int beats = 0, need = 2 * M_count;
    for (int t = 0; t < need * 8 + 40 && beats < need; t++) {
        g_dut->eval();
        if (g_dut->m_axis_tvalid && g_dut->m_axis_tready) {
            int m = beats / 2, half = beats % 2;
            for (int i = 0; i < VPB; i++) {
                int col = half * VPB + i;
                uint32_t bits = g_dut->m_axis_tdata[i];
                memcpy(&out[m * COLS + col], &bits, sizeof(float));
            }
            beats++;
        }
        tick();
    }
    if (beats < need)
        fprintf(stderr, "[systolic/vrl] WARNING: only %d/%d readback beats "
                "(apply_bias=%d apply_gelu=%d)\n", beats, need, ab, ag);

    g_dut->apply_bias    = 0;
    g_dut->apply_gelu    = 0;
    g_dut->m_axis_tready = 0; tick();
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
    g_accum_bank = 0; g_dut->accum_buf_sel = 0;
    g_hw_cycles_fwd = 0; g_hw_cycles_bwd = 0; g_tick_count = 0;
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
