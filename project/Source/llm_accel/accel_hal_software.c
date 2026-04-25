// =============================================================================
// accel_hal_software.c — Pure-C software simulation backend
//
// Mimics the hardware behavior: BF16 multiply, FP32 accumulate.
// Numerically matches what the RTL produces.  No Verilator required.
// =============================================================================

#include "accel_hal.h"
#include <stdio.h>
#include <string.h>

static float          sw_psum[ACCEL_VEC_SIZE];
static long long      sw_tile_count = 0;

void accel_hal_init(void) { /* nothing to initialise */ }
void accel_hal_free(void) { /* nothing to release   */ }

void hal_compute_tile(const bf16_t* w_tile, const bf16_t* act, int first_tile) {
    if (first_tile)
        for (int n = 0; n < ACCEL_VEC_SIZE; n++) sw_psum[n] = 0.0f;

    for (int k = 0; k < ACCEL_K_DEPTH; k++) {
        float a = bf16_to_float(act[k]);
        for (int n = 0; n < ACCEL_VEC_SIZE; n++)
            sw_psum[n] += a * bf16_to_float(w_tile[k * ACCEL_VEC_SIZE + n]);
    }
    sw_tile_count++;
}

void hal_read_results(float* out) {
    memcpy(out, sw_psum, ACCEL_VEC_SIZE * sizeof(float));
}

void accel_reset_timing(void) { sw_tile_count = 0; }

void accel_print_timing(void) {
    // Clock frequency from corrected synthesis: path = 1572ps → target 1650ps
    // → 606 MHz (custom BF16 FPU). HardFloat variant targets 571 MHz.
    double cycles   = (double)sw_tile_count * ACCEL_K_DEPTH;
    double wall_sec = cycles / 606e6;
    printf("[accel/software] tiles=%lld  projected_hw_cycles=%.0f"
           "  projected_hw_time=%.4f s  (@ 606 MHz BF16 / 571 MHz HardFloat)\n",
           sw_tile_count, cycles, wall_sec);
}
