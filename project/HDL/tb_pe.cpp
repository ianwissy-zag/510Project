// Verilator testbench for mac_pe — BF16 inputs, FP32 psum.
//
// All activations and weights are expressed as BF16 float values.
// BF16 is the upper 16 bits of the IEEE 754 FP32 bit pattern.
// Accumulators/psums are full FP32 (32-bit).
//
// Test 3: weight=BF16(42.0), act=BF16(3.0), psum_in=FP32(100.0)
//   psum_out = 100.0 + 3.0 * 42.0 = 226.0
//
// Test 4: same weight, act=BF16(5.0), psum_in=FP32(226.0)
//   psum_out = 226.0 + 5.0 * 42.0 = 436.0

#include "Vmac_pe.h"
#include "verilated.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

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

static void chk_raw(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got 0x%04X, expected 0x%04X\n", label, got, expected);
        g_errors++;
    }
}

static void tick(Vmac_pe* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vmac_pe* dut = new Vmac_pe{ctx};

    // BF16 and FP32 constants used across tests
    const uint16_t bf16_3   = to_bf16(3.0f);
    const uint16_t bf16_5   = to_bf16(5.0f);
    const uint16_t bf16_42  = to_bf16(42.0f);
    const uint16_t bf16_7   = to_bf16(7.0f);
    const uint32_t fp32_0   = to_fp32_bits(0.0f);
    const uint32_t fp32_100 = to_fp32_bits(100.0f);
    const uint32_t fp32_226 = to_fp32_bits(226.0f);  // 100 + 3*42
    const uint32_t fp32_436 = to_fp32_bits(436.0f);  // 226 + 5*42

    // ── Reset ────────────────────────────────────────────────────────────
    dut->clk = 0; dut->rst_n = 0;
    dut->load_wt = 0; dut->a_in = 0; dut->w_in = 0; dut->psum_in = 0;
    tick(dut); tick(dut);
    dut->rst_n = 1; dut->eval();

    // ── Test 1: post-reset all outputs are 0 ────────────────────────────
    printf("Test 1: Post-reset outputs\n");
    chk_fp32("a_out",    dut->a_out,    0);
    chk_fp32("w_out",    dut->w_out,    0);
    chk_fp32("psum_out", dut->psum_out, fp32_0);

    // ── Test 2: weight load shifts values top-to-bottom ─────────────────
    printf("Test 2: Weight shift chain\n");
    dut->load_wt = 1;
    dut->w_in = bf16_7;
    tick(dut);  // weight_reg=BF16(7.0), w_out=0
    chk_raw("w_out after 1st load", dut->w_out, 0);

    dut->w_in = bf16_42;
    tick(dut);  // weight_reg=BF16(42.0), w_out=BF16(7.0)
    chk_raw("w_out after 2nd load", dut->w_out, bf16_7);

    // ── Test 3: compute – BF16 MAC: 100.0 + 3.0*42.0 = 226.0 ───────────
    // weight_reg is now BF16(42.0)
    printf("Test 3: MAC compute (100.0 + 3.0 * 42.0 = 226.0)\n");
    dut->load_wt = 0;
    dut->a_in    = bf16_3;
    dut->psum_in = fp32_100;
    tick(dut);
    chk_fp32("psum_out", dut->psum_out, fp32_226);
    chk_raw ("a_out",    dut->a_out,    bf16_3);

    // ── Test 4: partial-sum accumulation: 226.0 + 5.0*42.0 = 436.0 ─────
    printf("Test 4: Partial-sum accumulation (226.0 + 5.0 * 42.0 = 436.0)\n");
    dut->a_in    = bf16_5;
    dut->psum_in = dut->psum_out;
    tick(dut);
    chk_fp32("psum_out (accumulated)", dut->psum_out, fp32_436);

    // ── Test 5: asynchronous reset clears all registers ──────────────────
    printf("Test 5: Async reset\n");
    dut->rst_n = 0;
    dut->eval();
    chk_raw ("a_out after reset",    dut->a_out,    0);
    chk_raw ("w_out after reset",    dut->w_out,    0);
    chk_fp32("psum_out after reset", dut->psum_out, fp32_0);
    dut->rst_n = 1; dut->eval();

    // ── Test 6: no compute occurs while load_wt is high ──────────────────
    printf("Test 6: load_wt gates compute\n");
    dut->load_wt = 1;
    dut->w_in = bf16_42;
    tick(dut);

    dut->a_in    = to_bf16(99.0f);
    dut->psum_in = fp32_100;
    tick(dut);  // still in load mode — psum_out stays 0
    chk_fp32("psum_out unchanged in load mode", dut->psum_out, fp32_0);

    if (g_errors == 0) printf("All PE tests PASSED.\n");
    else               printf("%d PE test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
