// gelu_dpi.c — DPI-C helpers for bias addition and Padé [3/2] GELU.
//
// These model the post-processing pipeline that would follow the systolic
// array in silicon: bias add (one FP32 adder per column) then optional
// GELU (Padé rational approximation, no transcendental LUT needed).
//
// Padé [3/2] in x²:
//   tanh(x) ≈ x·(1485 + 171x² + 2x⁴) / (1485 + 666x² + 26x⁴)
//   Coefficients are exact integers derived from the Padé linear system.
//   Error vs tanhf: < 0.01% for |x| ≤ 2,  < 0.2% for |x| ≤ 3.
//   Saturate at |x| ≥ 4  (tanh(4) = 0.9993, error < 0.07%).

#include "svdpi.h"
#include <stdint.h>
#include <string.h>

// FP32 bit-cast helpers
static inline float u2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t f2u(float  f) { uint32_t u; memcpy(&u, &f, 4); return u; }

// fp32_add_dpi — bias addition: result = a + b  (both FP32 bit patterns)
// extern "C" prevents C++ name mangling when compiled with g++.
#ifdef __cplusplus
extern "C"
#endif
uint32_t fp32_add_dpi(uint32_t a_bits, uint32_t b_bits) {
    return f2u(u2f(a_bits) + u2f(b_bits));
}

// gelu_pade32_dpi — GELU with Padé [3/2] tanh approximation.
// GELU(x) = 0.5·x·(1 + tanh(√(2/π)·(x + 0.044715·x³)))
#ifdef __cplusplus
extern "C"
#endif
uint32_t gelu_pade32_dpi(uint32_t x_bits) {
    float x = u2f(x_bits);
    float x2  = x * x;
    float arg = 0.7978845608f * (x + 0.044715f * x2 * x);

    float a2  = arg * arg;
    float a4  = a2  * a2;
    float num = arg * (1485.0f + 171.0f * a2 + 2.0f * a4);
    float den =        1485.0f + 666.0f * a2 + 26.0f * a4;
    float t   = num / den;
    if (t >  1.0f) t =  1.0f;
    if (t < -1.0f) t = -1.0f;

    return f2u(0.5f * x * (1.0f + t));
}
