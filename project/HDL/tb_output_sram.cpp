// Verilator testbench for output_sram
// DATA_WIDTH = 32*32 = 1024 bits = 32 x uint32_t words
#include "Voutput_sram.h"
#include "verilated.h"
#include <cstdio>

static int g_errors = 0;

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got %u, expected %u\n", label, got, expected);
        g_errors++;
    }
}

static void tick(Voutput_sram* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Voutput_sram* dut = new Voutput_sram{ctx};

    dut->clk = 0; dut->we = 0; dut->re = 0;
    dut->waddr = 0; dut->raddr = 0;
    for (int i = 0; i < 32; i++) dut->wdata[i] = 0; // 1024-bit = 32 words
    tick(dut);

    // ── Test 1: write one full psum row and read back ─────────────────────
    // Simulates the systolic array writing all 32 psum_out values at once.
    printf("Test 1: Write and read back a psum row\n");
    for (int col = 0; col < 32; col++) dut->wdata[col] = col * 7 + 1;
    dut->waddr = 0; dut->we = 1;
    tick(dut);
    dut->we = 0;

    dut->raddr = 0; dut->re = 1;
    tick(dut);
    dut->re = 0;

    char label[48];
    for (int col = 0; col < 32; col++) {
        snprintf(label, sizeof(label), "psum[%d]", col);
        chk(label, dut->rdata[col], (uint32_t)(col * 7 + 1));
    }

    // ── Test 2: multiple rows, verify no aliasing ─────────────────────────
    printf("Test 2: Multiple psum rows, no aliasing\n");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 32; col++) dut->wdata[col] = (uint32_t)(row * 1000 + col);
        dut->waddr = row; dut->we = 1;
        tick(dut);
    }
    dut->we = 0;

    for (int row = 0; row < 8; row++) {
        dut->raddr = row; dut->re = 1;
        tick(dut);
        dut->re = 0;
        snprintf(label, sizeof(label), "row%d col0",  row); chk(label, dut->rdata[0],  (uint32_t)(row * 1000));
        snprintf(label, sizeof(label), "row%d col31", row); chk(label, dut->rdata[31], (uint32_t)(row * 1000 + 31));
    }

    // ── Test 3: simultaneous write/read at different rows ─────────────────
    // Simulates draining previous tile while writing next tile's results.
    printf("Test 3: Simultaneous write row 15, read row 0\n");
    for (int col = 0; col < 32; col++) dut->wdata[col] = 0xCAFE0000 + col;
    dut->waddr = 15; dut->we = 1;
    dut->raddr = 0;  dut->re = 1;
    tick(dut);
    dut->we = 0; dut->re = 0;
    // rdata should reflect row 0, not the newly written row 15
    chk("simul read row0 col0", dut->rdata[0], (uint32_t)(0 * 1000 + 0));

    // Verify row 15 was written
    dut->raddr = 15; dut->re = 1;
    tick(dut);
    dut->re = 0;
    chk("row15 col0",  dut->rdata[0],  0xCAFE0000);
    chk("row15 col31", dut->rdata[31], 0xCAFE001F);

    // ── Test 4: read port inactive – rdata held ───────────────────────────
    printf("Test 4: Read port inactive holds rdata\n");
    uint32_t held = dut->rdata[0];
    dut->re = 0;
    tick(dut);
    chk("rdata held when re=0", dut->rdata[0], held);

    if (g_errors == 0) printf("All output SRAM tests PASSED.\n");
    else               printf("%d output SRAM test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
