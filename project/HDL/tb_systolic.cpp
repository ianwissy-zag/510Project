// Verilator testbench for systolic_array_32x32 — BF16 weights/acts, FP32 psums.
//
// BF16(1.0) = 0x3F80; two packed per uint32 word = 0x3F803F80.
// FP32(0.0) = 0x00000000  (psum init — unchanged)
// FP32(1.0) = 0x3F800000  (expected after single-row steady-state)
// FP32(32.0)= 0x42000000  (expected after all-rows steady-state)
//
// Test 3: act_in[row=0] = BF16(1.0), all others 0, all weights BF16(1.0)
//   Steady-state: psum_out[col] = FP32(1.0) for all col
//
// Test 4: act_in[all rows] = BF16(1.0), all weights BF16(1.0)
//   Steady-state: psum_out[col] = FP32(32.0) for all col

#include "Vsystolic_array_32x32.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

static const int N = 32;
static int g_errors = 0;

static uint16_t to_bf16(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return (uint16_t)(bits >> 16);
}

static uint32_t to_fp32_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

static void chk_fp32(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        float got_f, exp_f;
        memcpy(&got_f, &got, 4);
        memcpy(&exp_f, &expected, 4);
        printf("  FAIL [%s]: got 0x%08X (%.4f)  expected 0x%08X (%.4f)\n",
               label, got, got_f, expected, exp_f);
        g_errors++;
    }
}

static void tick(Vsystolic_array_32x32* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

// act_in / wt_in: logic [N-1:0][15:0] — 512 bits = uint32_t[16]
// psum_in:        logic [N-1:0][31:0] — 1024 bits = uint32_t[32]
static void zero_inputs(Vsystolic_array_32x32* dut) {
    for (int i = 0; i < N / 2; i++) { dut->act_in[i] = 0; dut->wt_in[i] = 0; }
    for (int i = 0; i < N;     i++) dut->psum_in[i] = 0;
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vsystolic_array_32x32* dut = new Vsystolic_array_32x32{ctx};

    const uint16_t BF16_1   = to_bf16(1.0f);           // 0x3F80
    const uint32_t BF16_1x2 = ((uint32_t)BF16_1 << 16) | BF16_1; // 0x3F803F80
    const uint32_t FP32_0   = to_fp32_bits(0.0f);      // 0x00000000
    const uint32_t FP32_1   = to_fp32_bits(1.0f);      // 0x3F800000
    const uint32_t FP32_N   = to_fp32_bits((float)N);  // 0x42000000

    // ── Reset ────────────────────────────────────────────────────────────
    dut->clk = 0; dut->rst_n = 0; dut->load_wt = 0;
    zero_inputs(dut);
    dut->eval();
    for (int i = 0; i < 4; i++) { dut->clk ^= 1; dut->eval(); }
    dut->clk = 0; dut->rst_n = 1; dut->eval();

    // ── Test 1: post-reset all psum outputs are FP32(0.0) ───────────────
    printf("Test 1: Post-reset psum outputs\n");
    char label[64];
    for (int col = 0; col < N; col++) {
        snprintf(label, sizeof(label), "psum_out[%d] at reset", col);
        chk_fp32(label, dut->psum_out[col], FP32_0);
    }

    // ── Test 2: load all weights = BF16(1.0) ────────────────────────────
    // Two BF16(1.0) values packed per 32-bit word.
    // Weights ripple one row per cycle; full 32-row load = 2*N cycles.
    printf("Test 2: Weight load\n");
    dut->load_wt = 1;
    for (int i = 0; i < N / 2; i++) dut->wt_in[i] = BF16_1x2;
    for (int c = 0; c < 2 * N; c++) tick(dut);
    dut->load_wt = 0;

    // ── Test 3: steady-state, row 0 only: psum_out[col] = FP32(1.0) ────
    // act_in[row=0] = BF16(1.0) (packed in low 16 bits of word 0).
    // PE[0][col] : acc += 1.0 * 1.0 = 1.0; a_out passed down.
    // PE[r][col] for r>0: act from previous row's a_out = 0, psum_in = 1.0
    //   -> psum_out = 1.0 (passes through unchanged).
    printf("Test 3: Steady-state single-row activation (expect FP32 1.0)\n");
    zero_inputs(dut);
    dut->act_in[0] = (uint32_t)BF16_1;  // row 0 = BF16(1.0), row 1 = 0

    for (int c = 0; c < 2 * N; c++) tick(dut);

    for (int col = 0; col < N; col++) {
        snprintf(label, sizeof(label), "psum_out[%d]", col);
        chk_fp32(label, dut->psum_out[col], FP32_1);
    }

    // ── Test 4: all-rows BF16(1.0): psum_out[col] = FP32(32.0) ─────────
    // Every PE accumulates 1.0 per row: psum_out[col] = 32 × 1.0 = 32.0
    printf("Test 4: All-rows activation (expect FP32 32.0)\n");
    for (int i = 0; i < N / 2; i++) dut->act_in[i] = BF16_1x2;
    for (int c = 0; c < 2 * N; c++) tick(dut);

    for (int col = 0; col < N; col++) {
        snprintf(label, sizeof(label), "psum_out[%d] all-rows", col);
        chk_fp32(label, dut->psum_out[col], FP32_N);
    }

    // ── Test 5: async reset clears all partial sums ───────────────────────
    printf("Test 5: Async reset clears outputs\n");
    dut->rst_n = 0;
    dut->eval();
    for (int col = 0; col < N; col++) {
        snprintf(label, sizeof(label), "psum_out[%d] post-reset", col);
        chk_fp32(label, dut->psum_out[col], FP32_0);
    }
    dut->rst_n = 1; dut->eval();

    if (g_errors == 0) printf("All systolic array tests PASSED.\n");
    else               printf("%d systolic array test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
