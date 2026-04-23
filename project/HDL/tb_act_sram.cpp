// Verilator testbench for act_sram
#include "Vact_sram.h"
#include "verilated.h"
#include <cstdio>

static int g_errors = 0;

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got %u, expected %u\n", label, got, expected);
        g_errors++;
    }
}

static void tick(Vact_sram* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vact_sram* dut = new Vact_sram{ctx};

    dut->clk = 0; dut->we = 0; dut->re = 0;
    dut->waddr = 0; dut->raddr = 0;
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0;
    tick(dut);

    // ── Test 1: basic write then read ────────────────────────────────────
    printf("Test 1: Basic write then read\n");
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0xBEEF + i;
    dut->waddr = 10; dut->we = 1;
    tick(dut);
    dut->we = 0;

    dut->raddr = 10; dut->re = 1;
    tick(dut);
    dut->re = 0;

    char label[48];
    for (int i = 0; i < 16; i++) {
        snprintf(label, sizeof(label), "word[%d]", i);
        chk(label, dut->rdata[i], (uint32_t)(0xBEEF + i));
    }

    // ── Test 2: simultaneous write/read at different addresses ───────────
    // This is the core use case: AXI writes one buffer while the systolic
    // array controller reads the other (ping-pong bank separation).
    printf("Test 2: Simultaneous write addr 1, read addr 0\n");

    // Prime addr 0 with known data
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0x1000 + i;
    dut->waddr = 0; dut->we = 1;
    tick(dut);
    dut->we = 0;

    // Simultaneous: write new data to addr 1, read from addr 0
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0x2000 + i;
    dut->waddr = 1; dut->we = 1;
    dut->raddr = 0; dut->re = 1;
    tick(dut);
    dut->we = 0; dut->re = 0;

    for (int i = 0; i < 16; i++) {
        snprintf(label, sizeof(label), "simul_read addr0 word[%d]", i);
        chk(label, dut->rdata[i], (uint32_t)(0x1000 + i));
    }

    // Verify addr 1 also written correctly
    dut->raddr = 1; dut->re = 1;
    tick(dut);
    dut->re = 0;
    for (int i = 0; i < 16; i++) {
        snprintf(label, sizeof(label), "write_verify addr1 word[%d]", i);
        chk(label, dut->rdata[i], (uint32_t)(0x2000 + i));
    }

    // ── Test 3: fill all 32 rows and verify each ─────────────────────────
    printf("Test 3: Fill all 32 rows\n");
    for (int row = 0; row < 32; row++) {
        for (int i = 0; i < 16; i++) dut->wdata[i] = (uint32_t)(row * 100 + i);
        dut->waddr = row; dut->we = 1;
        tick(dut);
    }
    dut->we = 0;

    for (int row = 0; row < 32; row++) {
        dut->raddr = row; dut->re = 1;
        tick(dut);
        dut->re = 0;
        snprintf(label, sizeof(label), "row%d word[0]", row);
        chk(label, dut->rdata[0], (uint32_t)(row * 100));
        snprintf(label, sizeof(label), "row%d word[15]", row);
        chk(label, dut->rdata[15], (uint32_t)(row * 100 + 15));
    }

    // ── Test 4: read port inactive – rdata does not change ───────────────
    printf("Test 4: Read port inactive holds rdata\n");
    // After last tick above, rdata = row31 data; now tick with re=0
    uint32_t held = dut->rdata[0];
    dut->re = 0;
    tick(dut);
    chk("rdata held when re=0", dut->rdata[0], held);

    if (g_errors == 0) printf("All activation SRAM tests PASSED.\n");
    else               printf("%d activation SRAM test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
