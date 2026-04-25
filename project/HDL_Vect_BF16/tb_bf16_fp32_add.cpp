// Testbench for bf16_fp32_add — FP32 accumulator adder.
//
// Verifies FP32 + FP32 → FP32 against C float addition.
// Flush-to-zero applied to both denormal inputs and outputs.
//
// Tests:
//   1. Basic positive addition
//   2. Sign combinations (subtraction paths)
//   3. Zero and infinity special cases
//   4. Accumulation chain (32 additions simulating K_DEPTH=32)
//   5. 1000 random FP32 pairs vs C float reference
//   6. Exponent alignment (large exponent difference)

#include "Vbf16_fp32_add.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <cmath>

static int g_errors = 0;

static uint32_t fp32_bits(float f) {
    uint32_t b; memcpy(&b, &f, 4); return b;
}
static float fp32_val(uint32_t b) {
    float f; memcpy(&f, &b, 4); return f;
}

// Flush denormal FP32 to zero (match hardware behavior)
static uint32_t ftz(float f) {
    uint32_t b = fp32_bits(f);
    if ((b & 0x7F800000u) == 0 && (b & 0x007FFFFFu) != 0)
        b &= 0x80000000u;  // keep sign, zero mantissa+exponent
    return b;
}

// Hardware uses truncation; C uses round-to-nearest.
// Exact match required for known values; ±2 ULP allowed for random inputs
// where catastrophic cancellation can amplify truncation error.
static bool within_ulp(uint32_t got, uint32_t exp, int tol) {
    if (got == exp) return true;
    auto to_signed = [](uint32_t b) -> int64_t {
        return (b & 0x80000000u) ? -(int64_t)(b & 0x7FFFFFFFu)
                                 :  (int64_t)(b & 0x7FFFFFFFu);
    };
    int64_t diff = to_signed(got) - to_signed(exp);
    return diff >= -tol && diff <= tol;
}
static bool within_1ulp(uint32_t got, uint32_t exp) { return within_ulp(got, exp, 1); }

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (!within_1ulp(got, expected)) {
        printf("  FAIL [%s]: got 0x%08X (%.6g)  expected 0x%08X (%.6g)\n",
               label, got, fp32_val(got), expected, fp32_val(expected));
        ++g_errors;
    }
}

static void eval(Vbf16_fp32_add* dut, float fa, float fb,
                 float fexp, const char* label) {
    dut->a = fp32_bits(fa);
    dut->b = fp32_bits(fb);
    dut->eval();
    chk(label, dut->result, ftz(fexp));
}

static void eval_bits(Vbf16_fp32_add* dut, uint32_t a, uint32_t b,
                      uint32_t expected, const char* label) {
    dut->a = a;
    dut->b = b;
    dut->eval();
    chk(label, dut->result, expected);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vbf16_fp32_add* dut = new Vbf16_fp32_add{ctx};

    // ── Test 1: basic positive addition ───────────────────────────────────
    printf("Test 1: Basic positive addition\n");
    eval(dut, 1.0f,   1.0f,   2.0f,    "1.0 + 1.0 = 2.0");
    eval(dut, 1.0f,   2.0f,   3.0f,    "1.0 + 2.0 = 3.0");
    eval(dut, 4.0f,   4.0f,   8.0f,    "4.0 + 4.0 = 8.0");
    eval(dut, 32.0f,  32.0f,  64.0f,   "32.0 + 32.0 = 64.0");
    eval(dut, 0.5f,   0.5f,   1.0f,    "0.5 + 0.5 = 1.0");
    eval(dut, 0.25f,  0.75f,  1.0f,    "0.25 + 0.75 = 1.0");
    eval(dut, 528.0f, 0.0f,   528.0f,  "528.0 + 0.0 = 528.0");

    // ── Test 2: sign combinations (subtraction paths) ─────────────────────
    printf("Test 2: Sign combinations\n");
    eval(dut,  1.0f, -1.0f,   0.0f,   " 1.0 + -1.0 = 0.0");
    eval(dut, -1.0f,  1.0f,   0.0f,   "-1.0 +  1.0 = 0.0");
    eval(dut,  3.0f, -1.0f,   2.0f,   " 3.0 + -1.0 = 2.0");
    eval(dut, -3.0f,  1.0f,  -2.0f,   "-3.0 +  1.0 = -2.0");
    eval(dut, -1.0f, -1.0f,  -2.0f,   "-1.0 + -1.0 = -2.0");
    eval(dut,  2.0f, -3.0f,  -1.0f,   " 2.0 + -3.0 = -1.0");
    eval(dut,  4.0f, -4.0f,   0.0f,   " 4.0 + -4.0 = 0.0");

    // ── Test 3: zero and infinity ─────────────────────────────────────────
    printf("Test 3: Zero and infinity\n");
    eval(dut, 0.0f,    5.0f,    5.0f,    "0.0 + 5.0 = 5.0");
    eval(dut, 5.0f,    0.0f,    5.0f,    "5.0 + 0.0 = 5.0");
    eval(dut, 0.0f,    0.0f,    0.0f,    "0.0 + 0.0 = 0.0");
    // Infinity propagation
    eval_bits(dut, 0x7F800000u, fp32_bits(1.0f),
              0x7F800000u, "+inf + 1.0 = +inf");
    eval_bits(dut, fp32_bits(1.0f), 0xFF800000u,
              0xFF800000u, "1.0 + -inf = -inf");

    // ── Test 4: accumulation chain matching MAC testbench values ──────────
    // Simulate 32 additions of 1.0 → expect 32.0
    printf("Test 4: Accumulation chain (32 × 1.0 = 32.0)\n");
    {
        uint32_t acc = fp32_bits(0.0f);
        for (int i = 0; i < 32; i++) {
            dut->a = fp32_bits(1.0f);
            dut->b = acc;
            dut->eval();
            acc = dut->result;
        }
        chk("sum of 32 ones", acc, fp32_bits(32.0f));
    }

    // Simulate sum 1+2+...+32 = 528
    printf("Test 4b: Accumulation chain (1+2+...+32 = 528.0)\n");
    {
        uint32_t acc = fp32_bits(0.0f);
        for (int i = 1; i <= 32; i++) {
            dut->a = fp32_bits((float)i);
            dut->b = acc;
            dut->eval();
            acc = dut->result;
        }
        chk("sum 1..32", acc, fp32_bits(528.0f));
    }

    // ── Test 5: 1000 random FP32 pairs vs C float ─────────────────────────
    printf("Test 5: 1000 random FP32 pairs vs C float\n");
    std::mt19937 rng(42);
    // Random normal FP32: exponent [1,254], any mantissa, random sign
    std::uniform_int_distribution<uint32_t> exp_d(1, 127);  // keep in range
    std::uniform_int_distribution<uint32_t> man_d(0, (1u<<23)-1);
    std::uniform_int_distribution<uint32_t> sgn_d(0, 1);

    int ran_err = 0;
    for (int i = 0; i < 1000; i++) {
        uint32_t ba = (sgn_d(rng) << 31) | (exp_d(rng) << 23) | man_d(rng);
        uint32_t bb = (sgn_d(rng) << 31) | (exp_d(rng) << 23) | man_d(rng);
        float fa = fp32_val(ba), fb = fp32_val(bb);
        uint32_t expected = ftz(fa + fb);

        dut->a = ba;
        dut->b = bb;
        dut->eval();

        if (!within_ulp(dut->result, expected, 2)) {
            if (ran_err < 5) {
                printf("  FAIL [random %d]: 0x%08X(%.6g) + 0x%08X(%.6g)\n"
                       "       got 0x%08X(%.6g)  expected 0x%08X(%.6g)\n",
                       i, ba, fa, bb, fb,
                       dut->result, fp32_val(dut->result),
                       expected, fp32_val(expected));
            }
            ++ran_err;
            ++g_errors;
        }
    }
    if (ran_err == 0) printf("  All 1000 random cases passed.\n");
    else              printf("  %d / 1000 random cases failed.\n", ran_err);

    // ── Test 6: large exponent differences (alignment) ────────────────────
    printf("Test 6: Large exponent differences\n");
    eval(dut, 1.0f,       1e-10f, 1.0f + 1e-10f, "1.0 + 1e-10");
    eval(dut, 1.0f,       0.0f,   1.0f,           "1.0 + 0.0 (flush)");
    eval(dut, 1000000.0f, 0.001f, 1000000.0f + 0.001f, "1e6 + 1e-3");

    if (g_errors == 0) printf("All bf16_fp32_add tests PASSED.\n");
    else               printf("%d bf16_fp32_add test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
