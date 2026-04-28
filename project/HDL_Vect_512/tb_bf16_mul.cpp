// Testbench for bf16_mul — custom BF16×BF16 → FP32 multiplier.
//
// Verifies against C float multiplication (exact for BF16 inputs since
// 8×8 mantissa bits fit in FP32's 24-bit mantissa without rounding).
//
// Tests:
//   1. Basic positive values
//   2. Sign combinations
//   3. Zero inputs
//   4. Values from existing MAC testbenches (1.0..32.0)
//   5. 1000 random positive normal pairs vs C float reference
//   6. Overflow → infinity
//   7. Small values

#include "Vbf16_mul.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>

static int g_errors = 0;

static uint16_t to_bf16(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return (uint16_t)(bits >> 16);
}

static float from_bf16(uint16_t bf16) {
    uint32_t bits = (uint32_t)bf16 << 16;
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static uint32_t fp32_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        float gf, ef;
        memcpy(&gf, &got, 4);
        memcpy(&ef, &expected, 4);
        printf("  FAIL [%s]: got 0x%08X (%.6g)  expected 0x%08X (%.6g)\n",
               label, got, gf, expected, ef);
        ++g_errors;
    }
}

static void eval(Vbf16_mul* dut, uint16_t a, uint16_t b,
                 uint32_t expected, const char* label) {
    dut->act_bf16 = a;
    dut->wt_bf16  = b;
    dut->eval();
    chk(label, dut->product_fp32, expected);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vbf16_mul* dut = new Vbf16_mul{ctx};

    // ── Test 1: basic positive values ─────────────────────────────────────
    printf("Test 1: Basic positive values\n");
    eval(dut, to_bf16(1.0f),  to_bf16(1.0f),  fp32_bits(1.0f),  "1.0 × 1.0 = 1.0");
    eval(dut, to_bf16(1.0f),  to_bf16(2.0f),  fp32_bits(2.0f),  "1.0 × 2.0 = 2.0");
    eval(dut, to_bf16(2.0f),  to_bf16(3.0f),  fp32_bits(6.0f),  "2.0 × 3.0 = 6.0");
    eval(dut, to_bf16(4.0f),  to_bf16(4.0f),  fp32_bits(16.0f), "4.0 × 4.0 = 16.0");
    eval(dut, to_bf16(0.5f),  to_bf16(2.0f),  fp32_bits(1.0f),  "0.5 × 2.0 = 1.0");
    eval(dut, to_bf16(32.0f), to_bf16(32.0f), fp32_bits(1024.0f),"32.0 × 32.0 = 1024.0");

    // ── Test 2: sign combinations ─────────────────────────────────────────
    printf("Test 2: Sign combinations\n");
    eval(dut, to_bf16(-1.0f), to_bf16( 1.0f), fp32_bits(-1.0f), "-1.0 × +1.0 = -1.0");
    eval(dut, to_bf16( 1.0f), to_bf16(-1.0f), fp32_bits(-1.0f), "+1.0 × -1.0 = -1.0");
    eval(dut, to_bf16(-1.0f), to_bf16(-1.0f), fp32_bits( 1.0f), "-1.0 × -1.0 = +1.0");
    eval(dut, to_bf16(-2.0f), to_bf16( 3.0f), fp32_bits(-6.0f), "-2.0 × +3.0 = -6.0");
    eval(dut, to_bf16(-4.0f), to_bf16(-4.0f), fp32_bits(16.0f), "-4.0 × -4.0 = +16.0");

    // ── Test 3: zero inputs ───────────────────────────────────────────────
    printf("Test 3: Zero inputs\n");
    eval(dut, to_bf16(0.0f), to_bf16(1.0f),   fp32_bits(0.0f), "0.0 × 1.0 = 0.0");
    eval(dut, to_bf16(1.0f), to_bf16(0.0f),   fp32_bits(0.0f), "1.0 × 0.0 = 0.0");
    eval(dut, to_bf16(0.0f), to_bf16(0.0f),   fp32_bits(0.0f), "0.0 × 0.0 = 0.0");
    eval(dut, to_bf16(0.0f), to_bf16(128.0f), fp32_bits(0.0f), "0.0 × 128.0 = 0.0");

    // ── Test 4: values from MAC testbenches (1.0 to 32.0) ────────────────
    printf("Test 4: Values from MAC testbenches\n");
    for (int i = 1; i <= 32; i++) {
        char label[64];
        snprintf(label, sizeof(label), "1.0 × %d.0 = %d.0", i, i);
        eval(dut, to_bf16(1.0f), to_bf16((float)i), fp32_bits((float)i), label);
    }

    // ── Test 5: random positive normals vs C float reference ──────────────
    printf("Test 5: 1000 random positive normal pairs vs C float\n");
    std::mt19937 rng(42);
    // Positive normal BF16 range: exponent [1,254], any mantissa
    std::uniform_int_distribution<uint32_t> exp_dist(1, 254);
    std::uniform_int_distribution<uint32_t> man_dist(0, 127);

    int ran_errors = 0;
    for (int i = 0; i < 1000; i++) {
        // Build random positive normal BF16 values
        uint16_t a = (uint16_t)((exp_dist(rng) << 7) | man_dist(rng));
        uint16_t b = (uint16_t)((exp_dist(rng) << 7) | man_dist(rng));

        float fa = from_bf16(a);
        float fb = from_bf16(b);
        float ef = fa * fb;
        uint32_t expected = fp32_bits(ef);
        // Hardware uses flush-to-zero: denormal outputs become zero.
        // Check by testing if the FP32 exponent field is 0 with non-zero mantissa.
        if ((expected & 0x7F800000u) == 0 && (expected & 0x007FFFFFu) != 0)
            expected = (expected & 0x80000000u);  // preserve sign, zero mantissa+exp

        dut->act_bf16 = a;
        dut->wt_bf16  = b;
        dut->eval();

        if (dut->product_fp32 != expected) {
            if (ran_errors < 5) {
                float gf;
                memcpy(&gf, &dut->product_fp32, 4);
                printf("  FAIL [random %d]: BF16(0x%04X=%.6g) × BF16(0x%04X=%.6g)\n"
                       "       got 0x%08X (%.6g)  expected 0x%08X (%.6g)\n",
                       i, a, fa, b, fb,
                       dut->product_fp32, gf, expected, ef);
            }
            ++ran_errors;
            ++g_errors;
        }
    }
    if (ran_errors == 0) printf("  All 1000 random cases passed.\n");
    else                 printf("  %d / 1000 random cases failed.\n", ran_errors);

    // ── Test 6: overflow → infinity ───────────────────────────────────────
    printf("Test 6: Overflow → infinity\n");
    // BF16 max normal = 0x7F7F ≈ 3.39e38; product overflows FP32
    eval(dut, 0x7F7F, 0x7F7F, 0x7F800000, "BF16_max × BF16_max = +inf");
    eval(dut, 0xFF7F, 0x7F7F, 0xFF800000, "-BF16_max × BF16_max = -inf");

    // ── Test 7: small values and exponent boundary ────────────────────────
    printf("Test 7: Small values\n");
    eval(dut, to_bf16(0.5f),   to_bf16(0.5f),  fp32_bits(0.25f),  "0.5 × 0.5 = 0.25");
    eval(dut, to_bf16(0.25f),  to_bf16(4.0f),  fp32_bits(1.0f),   "0.25 × 4.0 = 1.0");
    eval(dut, to_bf16(0.125f), to_bf16(8.0f),  fp32_bits(1.0f),   "0.125 × 8.0 = 1.0");
    eval(dut, to_bf16(0.5f),   to_bf16(0.25f), fp32_bits(0.125f), "0.5 × 0.25 = 0.125");

    if (g_errors == 0) printf("All bf16_mul tests PASSED.\n");
    else               printf("%d bf16_mul test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
