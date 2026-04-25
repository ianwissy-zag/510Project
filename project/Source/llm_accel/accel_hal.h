// =============================================================================
// accel_hal.h — Hardware Abstraction Layer for the BF16 vector MAC accelerator
//
// Provides a C-compatible interface to three backends:
//   software   — pure C simulation of hardware behavior (default, fast)
//   vect128    — Verilator cycle-accurate sim, HDL_Vect  (HardFloat FPU)
//   bf16       — Verilator cycle-accurate sim, HDL_Vect_BF16 (custom FPU)
//
// Select backend at compile time:
//   -DACCEL_BACKEND_SOFTWARE    (default)
//   -DACCEL_BACKEND_VECT128
//   -DACCEL_BACKEND_BF16
//
// Limit to one training step (default for Verilator — glacially slow):
//   -DACCEL_SINGLE_PASS          (set by default in Makefile for Verilator)
// =============================================================================

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

// ── Accelerator parameters (must match RTL parameters) ────────────────────────
#define ACCEL_VEC_SIZE  128   // output channels per tile  (VEC_SIZE in RTL)
#define ACCEL_K_DEPTH   32    // inner-product depth       (K_DEPTH  in RTL)

// ── BF16 type and conversion ──────────────────────────────────────────────────
typedef uint16_t bf16_t;

static inline bf16_t float_to_bf16(float f) {
    uint32_t b; memcpy(&b, &f, sizeof(b)); return (bf16_t)(b >> 16);
}
static inline float bf16_to_float(bf16_t b) {
    uint32_t bits = (uint32_t)b << 16; float f; memcpy(&f, &bits, sizeof(f)); return f;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
// Call accel_hal_init() before any HAL operations, accel_hal_free() at exit.
void accel_hal_init(void);
void accel_hal_free(void);

// ── Tile operations ───────────────────────────────────────────────────────────
// w_tile  : K_DEPTH × VEC_SIZE BF16 values, row-major [k][n]
// act     : K_DEPTH BF16 values
// first_tile=1 seeds accumulators from zero; =0 accumulates onto previous result
void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile);

// Read VEC_SIZE FP32 results from the accumulator into out[].
void hal_read_results(float* out);

// ── Timing ────────────────────────────────────────────────────────────────────
void accel_reset_timing(void);
void accel_print_timing(void);

#ifdef __cplusplus
}
#endif
