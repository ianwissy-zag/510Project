// =============================================================================
// accel_hal_software.c — Pure-C software simulation backend
//
// Precision modes (compile-time, via -DPREC_*):
//   PREC_BF16_FP32  (default) BF16 multiply, FP32 accumulate
//   PREC_BF16_BF16            BF16 multiply, BF16 accumulate
//   PREC_INT16                INT16 multiply (per-tile quant), FP32 accumulate
//   PREC_INT8                 INT8  multiply (per-tile quant), FP32 accumulate
//   PREC_FP8_FP32             FP8 E4M3 multiply, FP32 accumulate
//
// Build:
//   make ACCEL_PREC=bf16_fp32    (default)
//   make ACCEL_PREC=bf16_bf16
//   make ACCEL_PREC=int16
//   make ACCEL_PREC=int8
//   make ACCEL_PREC=fp8
// =============================================================================

#include "accel_hal.h"
#include <stdio.h>
#include <string.h>

#if !defined(PREC_BF16_FP32) && !defined(PREC_BF16_BF16) && \
    !defined(PREC_INT16)     && !defined(PREC_INT8)       && \
    !defined(PREC_FP8_FP32)
#  define PREC_BF16_FP32
#endif

// ── Backend-specific tile dimensions ─────────────────────────────────────────
// Systolic backends use 32-column × 16-row tiles; vector backends use 128×32.
#if defined(ACCEL_BACKEND_SYSTOLIC) || defined(ACCEL_BACKEND_SYSTOLIC_VRL)
#  define HAL_N  ACCEL_SYS_COLS   // 32 — output channels per tile
#  define HAL_K  ACCEL_SYS_ROWS   // 16 — inner-product depth per tile
#else
#  define HAL_N  ACCEL_VEC_SIZE   // 128
#  define HAL_K  ACCEL_K_DEPTH    // 32
#endif

static float     sw_psum[HAL_N];
static long long sw_tile_count = 0;

void accel_hal_init(void) {}
void accel_hal_free(void) {}

// =============================================================================
#if defined(PREC_BF16_FP32)
// BF16 multiply, FP32 accumulate — matches RTL behavior exactly.

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < HAL_N; n++) sw_psum[n] = 0.0f;
    for (int k = 0; k < HAL_K; k++) {
        float a = bf16_to_float(act[k]);
        for (int n = 0; n < HAL_N; n++)
            sw_psum[n] += a * bf16_to_float(w_tile[k * HAL_N + n]);
    }
    sw_tile_count++;
}
static const char* prec_name = "BF16*BF16 -> FP32 accum (matches RTL)";

// =============================================================================
#elif defined(PREC_BF16_BF16)
// BF16 multiply, BF16 accumulate.
// Both product and running sum are truncated to BF16 after each operation.

static inline bf16_t fp32_to_bf16_trunc(float f) {
    unsigned int b; __builtin_memcpy(&b, &f, 4); return (bf16_t)(b >> 16);
}

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < HAL_N; n++) sw_psum[n] = 0.0f;
    for (int k = 0; k < HAL_K; k++) {
        float a = bf16_to_float(act[k]);
        for (int n = 0; n < HAL_N; n++) {
            float w    = bf16_to_float(w_tile[k * HAL_N + n]);
            float prod = bf16_to_float(fp32_to_bf16_trunc(a * w));
            float sum  = bf16_to_float(fp32_to_bf16_trunc(sw_psum[n] + prod));
            sw_psum[n] = sum;
        }
    }
    sw_tile_count++;
}
static const char* prec_name = "BF16*BF16 -> BF16 accum";

// =============================================================================
#elif defined(PREC_INT16)
// INT16 multiply with symmetric per-tile quantization, FP32 accumulate.
// Scale = max(|values in tile|) / 32767. Captures INT16 multiply noise.

#include <math.h>

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < HAL_N; n++) sw_psum[n] = 0.0f;

    float max_a = 0.0f, max_w = 0.0f;
    for (int k = 0; k < HAL_K; k++) {
        float a = fabsf(bf16_to_float(act[k]));
        if (a > max_a) max_a = a;
        for (int n = 0; n < HAL_N; n++) {
            float w = fabsf(bf16_to_float(w_tile[k * HAL_N + n]));
            if (w > max_w) max_w = w;
        }
    }
    float sa  = (max_a > 0.0f) ? max_a / 32767.0f : 1.0f;
    float sw  = (max_w > 0.0f) ? max_w / 32767.0f : 1.0f;
    float so  = sa * sw;

    for (int k = 0; k < HAL_K; k++) {
        short qa = (short)(bf16_to_float(act[k]) / sa + 0.5f);
        for (int n = 0; n < HAL_N; n++) {
            short qw = (short)(bf16_to_float(w_tile[k * HAL_N + n]) / sw + 0.5f);
            sw_psum[n] += (float)((int)qa * (int)qw) * so;
        }
    }
    sw_tile_count++;
}
static const char* prec_name = "INT16*INT16 -> FP32 accum (per-tile quant)";

// =============================================================================
#elif defined(PREC_INT8)
// INT8 multiply with symmetric per-tile quantization, FP32 accumulate.
// Scale = max(|values in tile|) / 127. Captures INT8 multiply noise.

#include <math.h>

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < HAL_N; n++) sw_psum[n] = 0.0f;

    float max_a = 0.0f, max_w = 0.0f;
    for (int k = 0; k < HAL_K; k++) {
        float a = fabsf(bf16_to_float(act[k]));
        if (a > max_a) max_a = a;
        for (int n = 0; n < HAL_N; n++) {
            float w = fabsf(bf16_to_float(w_tile[k * HAL_N + n]));
            if (w > max_w) max_w = w;
        }
    }
    float sa  = (max_a > 0.0f) ? max_a / 127.0f : 1.0f;
    float sw  = (max_w > 0.0f) ? max_w / 127.0f : 1.0f;
    float so  = sa * sw;

    for (int k = 0; k < HAL_K; k++) {
        signed char qa = (signed char)(bf16_to_float(act[k]) / sa + 0.5f);
        for (int n = 0; n < HAL_N; n++) {
            signed char qw = (signed char)(
                bf16_to_float(w_tile[k * HAL_N + n]) / sw + 0.5f);
            sw_psum[n] += (float)((int)qa * (int)qw) * so;
        }
    }
    sw_tile_count++;
}
static const char* prec_name = "INT8*INT8   -> FP32 accum (per-tile quant)";

#elif defined(PREC_FP8_FP32)
// FP8 E4M3 multiply, FP32 accumulate.
// OCP FP8 E4M3: 1 sign | 4 exp (bias=7) | 3 mantissa. Max normal = 448.

#include <stdint.h>

typedef uint8_t fp8_t;

static fp8_t float_to_fp8(float f) {
    unsigned int bits; __builtin_memcpy(&bits, &f, 4);
    int sign     = (bits >> 31) & 1;
    int exp32    = (bits >> 23) & 0xFF;
    unsigned int mant32 = bits & 0x7FFFFF;
    if (exp32 == 0 && mant32 == 0) return (fp8_t)(sign << 7);  // ±0
    int exp8 = (exp32 - 127) + 7;
    if (exp8 <= 0)  return (fp8_t)(sign << 7);           // underflow
    if (exp8 >= 15) return (fp8_t)((sign << 7) | 0x7E);  // saturate ±448
    unsigned int round = (mant32 >> 19) & 1;
    unsigned int mant8 = ((mant32 >> 20) + round) & 0x7;
    if (mant8 == 8) { mant8 = 0; exp8++; }
    if (exp8 >= 15) return (fp8_t)((sign << 7) | 0x7E);
    return (fp8_t)((sign << 7) | ((exp8 & 0xF) << 3) | mant8);
}

static float fp8_to_float(fp8_t fp8) {
    int sign  = (fp8 >> 7) & 1;
    int exp8  = (fp8 >> 3) & 0xF;
    int mant8 =  fp8       & 0x7;
    if (exp8 == 0 && mant8 == 0) return sign ? -0.0f : 0.0f;
    if (exp8 == 0) {  // subnormal
        float v = (float)mant8 / 8.0f;
        // 2^(-6) scaling without dividing by zero
        unsigned int e126 = (127 - 6) << 23;
        float scale; __builtin_memcpy(&scale, &e126, 4);
        return sign ? -(v * scale) : (v * scale);
    }
    unsigned int bits = ((unsigned int)sign << 31)
                      | ((unsigned int)(exp8 - 7 + 127) << 23)
                      | ((unsigned int)mant8 << 20);
    float f; __builtin_memcpy(&f, &bits, 4);
    return f;
}

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < HAL_N; n++) sw_psum[n] = 0.0f;
    for (int k = 0; k < HAL_K; k++) {
        float a = fp8_to_float(float_to_fp8(bf16_to_float(act[k])));
        for (int n = 0; n < HAL_N; n++) {
            float w = fp8_to_float(
                float_to_fp8(bf16_to_float(w_tile[k * HAL_N + n])));
            sw_psum[n] += a * w;
        }
    }
    sw_tile_count++;
}
static const char* prec_name = "FP8-E4M3*FP8-E4M3 -> FP32 accum";
#endif

// =============================================================================
// Common: readback and timing

// hal_compute_tile_bwd — backward dinp computation.
// For software backends the computation is identical to the forward pass:
// BF16 MAC of transposed weights against the gradient vector.
// The systolic Verilator backend overrides this with hardware-accurate
// simulation using the separate backward weight SRAM.
void hal_compute_tile_bwd(const bf16_t* wT_tile, const bf16_t* dout, int first_tile) {
    // Re-use forward tile logic: same arithmetic, different semantic meaning.
    hal_compute_tile(wT_tile, dout, first_tile);
}

void hal_read_results(float* out) {
    __builtin_memcpy(out, sw_psum, HAL_N * sizeof(float));
}

void accel_reset_timing(void) { sw_tile_count = 0; }

void accel_print_timing(void) {
    double cycles   = (double)sw_tile_count * HAL_K;
    double wall_sec = cycles / 606e6;
    printf("[accel/software] mode=%-42s tiles=%lld  projected_hw_time=%.4f s\n",
           prec_name, sw_tile_count, wall_sec);
}
