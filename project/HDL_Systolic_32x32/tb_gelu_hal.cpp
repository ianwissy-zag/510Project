// tb_gelu_hal.cpp
// HAL-level GELU testbench for HDL_Systolic_32x32.
//
// Exercises the gelu_unit and axi_readback GELU path through
// accel_hal_systolic_vrl — the same code path used in production inference.
//
// Strategy: an identity weight matrix (W[k][c] = δ(k,c)) makes output[m][c]
// equal to act[m][c] (BF16-rounded), so any specific GELU input value can be
// targeted by column.  The software reference is gelu_ref applied to the
// BF16-rounded activation so BF16 MAC rounding does not create false failures.
//
// Tests:
//   1. GELU accuracy across 32 distinct input values spanning all regions of
//      the GELU curve (large positive, small, zero, negative, saturation).
//   2. Multiple readbacks from the same result: hal_read_results_all (raw)
//      followed immediately by hal_read_results_all_gelu — validates that the
//      accum_buf_sel toggle guard in readback_all() prevents reading the wrong
//      accumulator bank on the second call.
//   3. M_count sweep: M=1, M=16, M=64 — first and last row checked.
//   4. Bias + GELU: hal_read_results_all_biased_gelu on a per-column bias.
//   5. Multi K-tile then GELU: 4 accumulated K-tiles → raw=128 → GELU(128)≈128.
//   6. Saturation: x=+8 and x=-6 where |arg|>>4 and tanh saturates to ±1.

#include "accel_hal.h"
#include <cstdio>
#include <cmath>
#include <cstring>

// Reference GELU using exact tanhf (for comparison with the Padé [3/2] hw).
static float gelu_ref(float x) {
    float cube = 0.044715f * x * x * x;
    return 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + cube)));
}

// Round-trip through BF16 (mirrors what the hardware MAC sees as input).
static float bf16r(float f) { return bf16_to_float(float_to_bf16(f)); }

// Scalar check; returns 1 on pass.
static int chk(const char* label, float got, float exp, float tol) {
    float err = fabsf(got - exp);
    if (err > tol) {
        printf("  FAIL %-26s  got=%10.5f  exp=%10.5f  err=%8.5f\n",
               label, got, exp, err);
        return 0;
    }
    return 1;
}

int main() {
    accel_hal_init();

    const int ROWS = ACCEL_SYS_ROWS;   // 32
    const int COLS = ACCEL_SYS_COLS;   // 32
    int all_pass = 1;

    // Build identity weight tile once: W[k][c] = 1 if k==c, else 0.
    bf16_t id_wt[ROWS * COLS];
    memset(id_wt, 0, sizeof(id_wt));
    for (int k = 0; k < ROWS; k++)
        id_wt[k * COLS + k] = float_to_bf16(1.0f);

    // All-ones weight tile (re-used by several tests).
    bf16_t ones_wt[ROWS * COLS];
    for (int i = 0; i < ROWS * COLS; i++) ones_wt[i] = float_to_bf16(1.0f);

    // ── Test 1: GELU accuracy across 32 distinct input values ─────────────────
    // Each column of the identity matmul passes one test value straight through.
    {
        printf("Test 1: GELU accuracy — 32 distinct input values\n");

        static const float tv[COLS] = {
            // large positive (tanh saturates, GELU ≈ x)
             8.0f,  6.0f,  4.5f,  4.0f,
            // moderate positive
             3.0f,  2.0f,  1.5f,  1.0f,
            // small positive
             0.5f,  0.25f, 0.125f, 0.0625f,
            // zero
             0.0f,
            // small negative
            -0.0625f, -0.125f, -0.25f, -0.5f,
            // moderate negative
            -1.0f, -1.5f, -2.0f, -2.5f,
            // large negative (tanh saturates, GELU ≈ 0)
            -3.0f, -4.0f, -4.5f, -6.0f,
            // supplementary spread
             0.75f, -0.75f, 1.25f, -1.25f,
             0.375f, -0.375f, 2.5f
        };

        bf16_t acts[ROWS];
        for (int k = 0; k < ROWS; k++) acts[k] = float_to_bf16(tv[k]);

        hal_stream_tile(id_wt, nullptr, nullptr, acts, 1, 1, 0);

        float hw[COLS];
        hal_read_results_all_gelu(hw, 1);

        int ok = 1;
        for (int c = 0; c < COLS; c++) {
            float in_bf16 = bf16r(tv[c]);
            float sw      = gelu_ref(in_bf16);
            char  lbl[40]; snprintf(lbl, sizeof(lbl), "col%02d(x=%.4f)", c, in_bf16);
            ok &= chk(lbl, hw[c], sw, 0.02f);
        }
        all_pass &= ok;
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    // ── Test 2: multiple readbacks — toggle guard ──────────────────────────────
    // hal_read_results_all then hal_read_results_all_gelu on the SAME accumulated
    // result.  Without the toggle guard the second call flips accum_buf_sel again
    // and reads the wrong (stale/empty) SRAM bank, producing wrong GELU output.
    {
        printf("Test 2: multiple readbacks on same result (toggle guard)\n");

        bf16_t acts[4 * ROWS];
        for (int i = 0; i < 4 * ROWS; i++) acts[i] = float_to_bf16(1.0f);

        hal_stream_tile(ones_wt, nullptr, nullptr, acts, 4, 1, 0);

        float raw_out[4 * COLS], gelu_out[4 * COLS];
        hal_read_results_all(raw_out, 4);        // first call — toggles bank
        hal_read_results_all_gelu(gelu_out, 4);  // second call — must NOT re-toggle

        float exp_raw  = (float)ROWS;            // 32 × (1×1) = 32
        float exp_gelu = gelu_ref(exp_raw);

        int ok = 1;
        for (int m = 0; m < 4; m++) {
            char lr[32], lg[32];
            snprintf(lr, sizeof(lr), "raw[%d][0]",  m);
            snprintf(lg, sizeof(lg), "gelu[%d][0]", m);
            ok &= chk(lr, raw_out[m * COLS],  exp_raw,  0.5f);
            ok &= chk(lg, gelu_out[m * COLS], exp_gelu, 0.1f);
        }
        all_pass &= ok;
        printf("  expected raw=%.1f  gelu=%.5f\n", exp_raw, exp_gelu);
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    // ── Test 3: M_count sweep (M=1, M=16, M=64) ───────────────────────────────
    {
        printf("Test 3: M_count sweep (M=1, M=16, M=64)\n");

        static const int m_vals[] = {1, 16, 64};
        float exp_gelu = gelu_ref((float)ROWS);  // W=1, act=1 → raw=ROWS always
        int ok = 1;

        for (int mi = 0; mi < 3; mi++) {
            int M = m_vals[mi];
            bf16_t acts[64 * ROWS];
            for (int i = 0; i < M * ROWS; i++) acts[i] = float_to_bf16(1.0f);

            hal_stream_tile(ones_wt, nullptr, nullptr, acts, M, 1, 0);

            float hw[64 * COLS];
            hal_read_results_all_gelu(hw, M);

            char l0[32], lN[32];
            snprintf(l0, sizeof(l0), "M=%d row0",   M);
            snprintf(lN, sizeof(lN), "M=%d row%d", M, M-1);
            ok &= chk(l0, hw[0],             exp_gelu, 0.1f);
            ok &= chk(lN, hw[(M-1) * COLS],  exp_gelu, 0.1f);
        }
        all_pass &= ok;
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    // ── Test 4: bias + GELU ────────────────────────────────────────────────────
    // W = 1/ROWS, act = 1  →  raw ≈ 1.0 per column.
    // bias[c] = c * 0.125  →  biased[c] = 1.0 + c * 0.125.
    // Expected: GELU(1.0 + c * 0.125) for each column.
    {
        printf("Test 4: bias + GELU (per-column bias)\n");

        float scale = 1.0f / (float)ROWS;
        bf16_t w_scaled[ROWS * COLS];
        for (int i = 0; i < ROWS * COLS; i++) w_scaled[i] = float_to_bf16(scale);

        bf16_t acts[4 * ROWS];
        for (int i = 0; i < 4 * ROWS; i++) acts[i] = float_to_bf16(1.0f);

        float bias[COLS];
        for (int c = 0; c < COLS; c++) bias[c] = (float)c * 0.125f;
        hal_load_bias(bias, COLS);

        hal_stream_tile(w_scaled, nullptr, nullptr, acts, 4, 1, 0);

        float hw[4 * COLS];
        hal_read_results_all_biased_gelu(hw, 4);

        int ok = 1;
        static const int tc[] = {0, 1, 8, 15, 16, 31};
        for (int m = 0; m < 4; m++) {
            for (int ci = 0; ci < 6; ci++) {
                int c = tc[ci];
                // 32 × (1/32) × 1 = 1.0 — exact in BF16 sum; bias is FP32
                float exp = gelu_ref(1.0f + bias[c]);
                char lbl[32]; snprintf(lbl, sizeof(lbl), "[m=%d c=%d]", m, c);
                ok &= chk(lbl, hw[m * COLS + c], exp, 0.05f);
            }
        }
        all_pass &= ok;
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    // ── Test 5: multi K-tile then GELU ────────────────────────────────────────
    // 4 K-tiles (W=1, act=1) accumulate to raw = 4*ROWS = 128.
    // GELU(128) is deep in the saturated region → ≈ 128.
    {
        printf("Test 5: 4 K-tile accumulation then GELU (raw=128)\n");

        bf16_t acts[4 * ROWS];
        for (int i = 0; i < 4 * ROWS; i++) acts[i] = float_to_bf16(1.0f);

        hal_stream_tile(ones_wt, nullptr, nullptr, acts, 4, 1, 0);
        hal_stream_tile(ones_wt, nullptr, nullptr, acts, 4, 0, 0);
        hal_stream_tile(ones_wt, nullptr, nullptr, acts, 4, 0, 0);
        hal_stream_tile(ones_wt, nullptr, nullptr, acts, 4, 0, 0);

        float hw[4 * COLS];
        hal_read_results_all_gelu(hw, 4);

        float exp_gelu = gelu_ref(4.0f * (float)ROWS);
        printf("  expected GELU(128) = %.5f\n", exp_gelu);

        int ok = 1;
        for (int m = 0; m < 4; m++) {
            char lbl[32]; snprintf(lbl, sizeof(lbl), "row%d[0]", m);
            ok &= chk(lbl, hw[m * COLS], exp_gelu, 0.5f);
        }
        all_pass &= ok;
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    // ── Test 6: GELU saturation boundaries ────────────────────────────────────
    // col 0: x = +8.0  → arg >> 4, tanh = +1, GELU ≈ x = 8.0
    // col 1: x = -6.0  → arg << -4, tanh = -1, GELU ≈ 0.0
    // col 2: x = +4.0  → boundary case (arg ≈ 5.5, just saturated)
    // col 3: x = -4.0  → boundary case (arg ≈ -5.5, just saturated)
    {
        printf("Test 6: saturation boundaries (|arg| >= 4)\n");

        bf16_t acts[ROWS];
        memset(acts, 0, sizeof(acts));
        acts[0] = float_to_bf16( 8.0f);
        acts[1] = float_to_bf16(-6.0f);
        acts[2] = float_to_bf16( 4.0f);
        acts[3] = float_to_bf16(-4.0f);

        hal_stream_tile(id_wt, nullptr, nullptr, acts, 1, 1, 0);

        float hw[COLS];
        hal_read_results_all_gelu(hw, 1);

        int ok = 1;
        ok &= chk("col0 x=+8.0", hw[0], gelu_ref( 8.0f), 0.05f);
        ok &= chk("col1 x=-6.0", hw[1], gelu_ref(-6.0f), 0.01f);
        ok &= chk("col2 x=+4.0", hw[2], gelu_ref( 4.0f), 0.05f);
        ok &= chk("col3 x=-4.0", hw[3], gelu_ref(-4.0f), 0.01f);
        printf("  sw: GELU(+8)=%.5f GELU(-6)=%.6f GELU(+4)=%.5f GELU(-4)=%.6f\n",
               gelu_ref(8.0f), gelu_ref(-6.0f), gelu_ref(4.0f), gelu_ref(-4.0f));
        all_pass &= ok;
        printf("  %s\n", ok ? "PASS" : "FAIL");
    }

    printf("\n[tb_gelu_hal] Overall: %s\n",
           all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    accel_hal_free();
    return all_pass ? 0 : 1;
}
