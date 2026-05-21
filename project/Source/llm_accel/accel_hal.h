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
#define ACCEL_SYS_ROWS  32    // K_DEPTH (array rows)

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

// ── Forward tile — single-row interface (vector/software backends) ────────────
// w_tile  : K_DEPTH × VEC_SIZE (or SYS_ROWS × SYS_COLS) BF16, row-major
// act     : K_DEPTH (or SYS_ROWS) BF16 activation values
// first_tile=1 seeds accumulators from zero
void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile);

// Read VEC_SIZE (or SYS_COLS) FP32 results into out[].
void hal_read_results(float* out);

// ── Streaming interface (systolic_vrl backend) ────────────────────────────────
// Loads one K-tile of weights then streams M_count activation rows in one pass.
// w_tile : SYS_ROWS × SYS_COLS BF16, packed row-major
// acts   : M_count × SYS_ROWS BF16, one row of K-elements per M row
// first_k: 1 = reset accumulator (first K-tile), 0 = accumulate
void hal_stream_tile(const bf16_t* w_tile, const bf16_t* acts,
                     int M_count, int first_k, int mode);

// Load N FP32 bias values into the hardware bias SRAM (N must equal SYS_COLS).
// Must be called before any readback that uses apply_bias=1.
void hal_load_bias(const float* bias, int N);

// Readback variants — accum_sram is read-only so multiple calls from the same
// accumulated result are safe (bias+GELU are applied on the way out only).
void hal_read_results_all(float* out, int M_count);             // raw
void hal_read_results_all_gelu(float* out, int M_count);        // GELU(raw)
void hal_read_results_all_biased(float* out, int M_count);      // raw+bias = fch
void hal_read_results_all_biased_gelu(float* out, int M_count); // GELU(raw+bias) = fch_gelu

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
