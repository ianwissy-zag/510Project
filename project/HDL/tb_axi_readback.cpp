// Verilator testbench for axi_readback.sv
// Models output_sram as a C++ array and verifies the AXI-S stream.
#include "Vaxi_readback.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>

static const int DEPTH      = 32;
static const int ARRAY_SIZE = 32;
static int g_errors = 0;

static void chk(const char* lbl, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got 0x%08x expected 0x%08x\n", lbl, got, expected);
        ++g_errors;
    }
}

static void tick(Vaxi_readback* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

// C++ model of output_sram (1024-bit rows = 32 × uint32_t words).
// Registered read: data appears one cycle after we see re=1 and raddr.
static uint32_t osram[DEPTH][ARRAY_SIZE];
static uint32_t sram_rdata_model[ARRAY_SIZE];

static void osram_tick(uint32_t addr, bool re) {
    if (re)
        for (int i = 0; i < ARRAY_SIZE; i++)
            sram_rdata_model[i] = osram[addr][i];
}

static void drive_sram_rdata(Vaxi_readback* dut) {
    for (int i = 0; i < ARRAY_SIZE; i++)
        dut->sram_rdata[i] = sram_rdata_model[i];
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vaxi_readback* dut = new Vaxi_readback{ctx};

    // Fill output SRAM: row r, col c = r*100 + c
    for (int r = 0; r < DEPTH; r++)
        for (int c = 0; c < ARRAY_SIZE; c++)
            osram[r][c] = (uint32_t)(r * 100 + c);
    memset(sram_rdata_model, 0, sizeof(sram_rdata_model));

    // ── Reset ─────────────────────────────────────────────────────────────
    dut->rst_n          = 0;
    dut->start          = 0;
    dut->m_axis_tready  = 0;
    drive_sram_rdata(dut);
    tick(dut); tick(dut);
    dut->rst_n = 1; dut->eval();

    // ── Test 1: idle — no valid before start ──────────────────────────────
    printf("Test 1: Idle — no tvalid\n");
    tick(dut);
    chk("idle tvalid", (uint32_t)dut->m_axis_tvalid, 0);
    chk("idle busy",   (uint32_t)dut->busy,           0);

    // ── Test 2: stream all rows with tready=1 throughout ──────────────────
    printf("Test 2: Stream all %d rows (tready always high)\n", DEPTH);
    dut->m_axis_tready = 1;
    dut->start         = 1;
    tick(dut);
    dut->start = 0;

    // Each row: FETCH cycle (1) + BEAT0 (1) + BEAT1 (1) = 3 cycles per row
    // Total = DEPTH * 3 cycles; budget some extra cycles
    int beats_received = 0;
    int last_beat      = -1;
    bool saw_last      = false;

    for (int t = 0; t < DEPTH * 4 + 10; t++) {
        // Model the registered SRAM output
        osram_tick((uint32_t)dut->sram_raddr, (bool)dut->sram_re);
        drive_sram_rdata(dut);
        dut->eval();

        if (dut->m_axis_tvalid && dut->m_axis_tready) {
            int row  = beats_received / 2;
            int beat = beats_received % 2;
            // Verify lower or upper half
            uint32_t expected_word0, expected_word15;
            if (beat == 0) {
                expected_word0  = osram[row][0];
                expected_word15 = osram[row][15];
            } else {
                expected_word0  = osram[row][16];
                expected_word15 = osram[row][31];
            }
            // tdata is 512-bit = 16 × uint32_t words in Verilator
            char lbl[48];
            snprintf(lbl, sizeof(lbl), "row%d beat%d word0",  row, beat);
            chk(lbl, dut->m_axis_tdata[0],  expected_word0);
            snprintf(lbl, sizeof(lbl), "row%d beat%d word15", row, beat);
            chk(lbl, dut->m_axis_tdata[15], expected_word15);

            if (dut->m_axis_tlast) {
                last_beat = beats_received;
                saw_last  = true;
            }
            ++beats_received;
        }

        tick(dut);
        if (!dut->busy && beats_received > 0) break;
    }

    int expected_beats = DEPTH * 2;
    if (beats_received != expected_beats) {
        printf("  FAIL: received %d beats, expected %d\n", beats_received, expected_beats);
        ++g_errors;
    }
    if (!saw_last) {
        printf("  FAIL: tlast never asserted\n");
        ++g_errors;
    } else if (last_beat != expected_beats - 1) {
        printf("  FAIL: tlast on beat %d, expected beat %d\n",
               last_beat, expected_beats - 1);
        ++g_errors;
    }

    // ── Test 3: back-pressure — tready deasserted for 3 cycles mid-stream ─
    printf("Test 3: Back-pressure test\n");
    dut->m_axis_tready = 1;
    dut->start = 1; tick(dut); dut->start = 0;

    int bp_beats   = 0;
    bool stall_applied = false;
    for (int t = 0; t < DEPTH * 8 + 20; t++) {
        osram_tick((uint32_t)dut->sram_raddr, (bool)dut->sram_re);
        drive_sram_rdata(dut);
        dut->eval();

        if (dut->m_axis_tvalid) {
            // stall after 2nd beat
            if (bp_beats == 2 && !stall_applied) {
                dut->m_axis_tready = 0;
                stall_applied = true;
            }
        }
        if (stall_applied && t % 3 == 0) dut->m_axis_tready = 1;

        if (dut->m_axis_tvalid && dut->m_axis_tready)
            ++bp_beats;

        tick(dut);
        if (!dut->busy && bp_beats > 0) break;
    }
    if (bp_beats != expected_beats) {
        printf("  FAIL: back-pressure: got %d beats expected %d\n",
               bp_beats, expected_beats);
        ++g_errors;
    }

    // ── Test 4: busy deasserts after stream completes ─────────────────────
    printf("Test 4: busy deasserts after last beat\n");
    chk("busy after done", (uint32_t)dut->busy, 0);

    if (g_errors == 0) printf("All AXI readback tests PASSED.\n");
    else               printf("%d AXI readback test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
