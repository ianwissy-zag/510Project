// Verilator testbench for weight_sram
// Uses DEPTH=64 override (-GDEPTH=64) set in the Makefile.
#include "Vweight_sram.h"
#include "verilated.h"
#include <cstdio>

static int g_errors = 0;

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got %u, expected %u\n", label, got, expected);
        g_errors++;
    }
}

static void tick(Vweight_sram* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

// Write one 512-bit row: word[i] = base + i
static void write_row(Vweight_sram* dut, uint32_t addr, uint32_t base) {
    for (int i = 0; i < 16; i++) dut->wdata[i] = base + i;
    dut->addr = addr;
    dut->we   = 1;
    tick(dut);
    dut->we   = 0;
}

// Issue a read; rdata is valid after tick() returns (registered output)
static void read_row(Vweight_sram* dut, uint32_t addr) {
    dut->addr = addr;
    dut->we   = 0;
    tick(dut);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vweight_sram* dut = new Vweight_sram{ctx};

    dut->clk = 0; dut->we = 0; dut->addr = 0;
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0;
    tick(dut);

    // ── Test 1: write then read back a single row ─────────────────────────
    printf("Test 1: Write and read back single row\n");
    write_row(dut, 5, 0xAA00);
    read_row(dut,  5);
    char label[48];
    for (int i = 0; i < 16; i++) {
        snprintf(label, sizeof(label), "row5 word[%d]", i);
        chk(label, dut->rdata[i], 0xAA00 + i);
    }

    // ── Test 2: multiple rows with distinct patterns – verify no aliasing ──
    printf("Test 2: Multiple rows, no aliasing\n");
    for (int row = 0; row < 8; row++)
        write_row(dut, row, (uint32_t)(row * 0x100));

    for (int row = 0; row < 8; row++) {
        read_row(dut, row);
        snprintf(label, sizeof(label), "row%d word[0]", row);
        chk(label, dut->rdata[0], (uint32_t)(row * 0x100));
        snprintf(label, sizeof(label), "row%d word[15]", row);
        chk(label, dut->rdata[15], (uint32_t)(row * 0x100 + 15));
    }

    // ── Test 3: overwrite a row and verify new data ───────────────────────
    printf("Test 3: Overwrite row\n");
    write_row(dut, 3, 0xBEEF00);
    read_row(dut,  3);
    chk("overwrite word[0]",  dut->rdata[0],  0xBEEF00);
    chk("overwrite word[15]", dut->rdata[15], 0xBEEF0F);

    // ── Test 4: read-before-write – rdata returns old value on same-cycle write
    printf("Test 4: Read-before-write behaviour\n");
    write_row(dut, 10, 0x1111);  // prime addr 10
    // Now write new data to addr 10 and read simultaneously (we=1)
    for (int i = 0; i < 16; i++) dut->wdata[i] = 0x9999 + i;
    dut->addr = 10; dut->we = 1;
    tick(dut);      // rdata captures old value (read-before-write)
    dut->we = 0;
    chk("read-before-write word[0]", dut->rdata[0], 0x1111);

    // Confirm the write did land
    read_row(dut, 10);
    chk("post-write word[0]", dut->rdata[0], 0x9999);

    if (g_errors == 0) printf("All weight SRAM tests PASSED.\n");
    else               printf("%d weight SRAM test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
