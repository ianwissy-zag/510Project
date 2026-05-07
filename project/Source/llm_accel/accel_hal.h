// =============================================================================
// accel_hal.h — Hardware Abstraction Layer for BF16 accelerator backends
//
// Backends (select at compile time):
//   -DACCEL_BACKEND_SOFTWARE       pure-C simulation (default, fast)
//   -DACCEL_BACKEND_VECT128        Verilator, HDL_Vect  (HardFloat FPU)
//   -DACCEL_BACKEND_BF16           Verilator, HDL_Vect_BF16 (custom FPU)
//   -DACCEL_BACKEND_SYSTOLIC       software sim of 16×32 systolic
//   -DACCEL_BACKEND_SYSTOLIC_VRL   Verilator, HDL_Systolic_16x32
//
// The systolic backends additionally accelerate the backward dinp matmul
// using the transposed weight SRAM and mode=BACKWARD control signal.
// =============================================================================

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

// ── Vector MAC parameters (HDL_Vect*, software modes) ────────────────────────
#define ACCEL_VEC_SIZE  128   // output channels per tile
#define ACCEL_K_DEPTH   32    // inner-product depth

// ── Systolic array parameters (HDL_Systolic_16x32) ───────────────────────────
#define ACCEL_SYS_COLS  32    // output channels per tile (array columns)
#define ACCEL_SYS_ROWS  16    // K_DEPTH (array rows)

// ── BF16 helpers ──────────────────────────────────────────────────────────────
typedef uint16_t bf16_t;

static inline bf16_t float_to_bf16(float f) {
    uint32_t b; memcpy(&b, &f, sizeof(b)); return (bf16_t)(b >> 16);
}
static inline float bf16_to_float(bf16_t b) {
    uint32_t bits = (uint32_t)b << 16; float f; memcpy(&f, &bits, sizeof(f)); return f;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void accel_hal_init(void);
void accel_hal_free(void);

// ── Forward tile (all backends) ───────────────────────────────────────────────
// w_tile  : K_DEPTH × VEC_SIZE (or SYS_ROWS × SYS_COLS) BF16, row-major
// act     : K_DEPTH (or SYS_ROWS) BF16 activation values
// first_tile=1 seeds accumulators from zero
void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile);

// Read VEC_SIZE (or SYS_COLS) FP32 results into out[].
void hal_read_results(float* out);

// ── Backward dinp tile (systolic backends only) ───────────────────────────────
// wT_tile : SYS_ROWS × SYS_COLS BF16 values of the TRANSPOSED weight matrix
// dout    : SYS_ROWS BF16 gradient values
// Computes dinp contribution: dinp[n] += sum_k(dout[k] * W^T[k][n])
// Non-systolic backends fall back to the same BF16 MAC computation.
void hal_compute_tile_bwd(const bf16_t* wT_tile, const bf16_t* dout, int first_tile);

// ── Timing ────────────────────────────────────────────────────────────────────
void accel_reset_timing(void);
void accel_print_timing(void);

#ifdef __cplusplus
}
#endif
