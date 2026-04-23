// Verilator testbench for controller.sv
// Models weight_sram and act_sram as plain C++ arrays.
// Verifies: state-machine timing, wt_addr schedule, act_re gating,
// out_we pulse, out_wdata contents, and done flag.
#include "Vcontroller.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>

static const int N         = 32;
static const int LOAD_CYC  = 2 * N;   // 64
static const int COMP_CYC  = 2 * N;   // 64
static int g_errors        = 0;

static void chk(const char* lbl, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got %u expected %u\n", lbl, got, expected);
        ++g_errors;
    }
}

static void tick(Vcontroller* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
}

// C++ model of weight_sram (512-bit rows = 32 × uint16_t words).
// Registered output: data is updated on the rising edge that sees addr/we.
static uint16_t wt_mem[N][N];
static uint16_t wt_rdata_model[N];

static void wt_sram_tick(uint32_t addr, const uint16_t wdata[N], bool we) {
    if (we) for (int i = 0; i < N; i++) wt_mem[addr][i] = wdata[i];
    // registered read
    for (int i = 0; i < N; i++) wt_rdata_model[i] = wt_mem[addr][i];
}

// C++ model of act_sram (512-bit rows, depth 32).
static uint16_t act_mem[N][N];
static uint16_t act_rdata_model[N];

static void act_sram_tick(uint32_t addr, bool re) {
    if (re) for (int i = 0; i < N; i++) act_rdata_model[i] = act_mem[addr][i];
}

// Drive wt_rdata port from the model array.
// wt_rdata is 512-bit = uint32_t[16]; two 16-bit elements packed per word.
static void drive_wt_rdata(Vcontroller* dut) {
    for (int w = 0; w < N/2; w++)
        dut->wt_rdata[w] = ((uint32_t)wt_rdata_model[2*w+1] << 16)
                          |  (uint32_t)wt_rdata_model[2*w];
}
static void drive_act_rdata(Vcontroller* dut) {
    for (int w = 0; w < N/2; w++) {
        uint32_t word = ((uint32_t)act_rdata_model[2*w+1] << 16)
                       |  (uint32_t)act_rdata_model[2*w];
        dut->act_rdata_0[w] = word;
        dut->act_rdata_1[w] = word;
    }
}

// Manually drive psum_out with known values so we can verify capture.
// psum_out[c] = c + 1  (arbitrary but distinct)
static void drive_psum_out(Vcontroller* dut) {
    for (int c = 0; c < N; c++) dut->psum_out[c] = (uint32_t)(c + 1);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vcontroller* dut = new Vcontroller{ctx};

    // Pre-fill weight SRAM: row r = [r, r, r, ..., r]  (all N words = r)
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            wt_mem[r][c] = (uint16_t)r;

    // Pre-fill act SRAM row 0: [1, 1, 1, ..., 1]
    for (int c = 0; c < N; c++) act_mem[0][c] = 1;

    // Initial rdata models are zero
    memset(wt_rdata_model,  0, sizeof(wt_rdata_model));
    memset(act_rdata_model, 0, sizeof(act_rdata_model));

    // ── Reset ─────────────────────────────────────────────────────────────
    dut->rst_n       = 0;
    dut->start       = 0;
    dut->act_buf_sel = 0;
    dut->first_tile  = 1;
    dut->last_tile   = 1;
    drive_wt_rdata(dut);
    drive_act_rdata(dut);
    drive_psum_out(dut);
    for (int i = 0; i < N; i++) dut->psum_out[i] = 0;
    tick(dut); tick(dut);
    dut->rst_n = 1; dut->eval();

    // ── Test 1: idle — no activity before start ───────────────────────────
    printf("Test 1: Idle state — no activity\n");
    tick(dut);
    chk("idle load_wt",  (uint32_t)dut->load_wt,  0);
    chk("idle out_we",   (uint32_t)dut->out_we,    0);
    chk("idle done",     (uint32_t)dut->done,       0);

    // ── Test 2: LOAD_WT phase timing and wt_addr schedule ────────────────
    printf("Test 2: LOAD_WT phase — 64 cycles, descending address\n");
    dut->start = 1; tick(dut); dut->start = 0;
    // start pulse sent; state should now be LOAD_WT on next posedge
    // run 64 cycles, verify load_wt=1 and address schedule
    int addr_errors = 0;
    for (int t = 0; t < LOAD_CYC; t++) {
        uint32_t expected_addr = (uint32_t)(N - 1 - (t >> 1));
        if (dut->load_wt != 1) {
            printf("  FAIL: load_wt=0 at load cycle %d\n", t);
            ++g_errors;
        }
        if ((uint32_t)dut->wt_addr != expected_addr && addr_errors < 4) {
            printf("  FAIL: wt_addr=%u expected %u at t=%d\n",
                   (uint32_t)dut->wt_addr, expected_addr, t);
            ++g_errors; ++addr_errors;
        }
        // Simulate registered SRAM
        wt_sram_tick((uint32_t)dut->wt_addr, nullptr, false);
        drive_wt_rdata(dut);
        act_sram_tick(0, (bool)(dut->act_re_0 || dut->act_re_1));
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }

    // ── Test 3: COMPUTE phase — load_wt=0, act_re asserted ───────────────
    printf("Test 3: COMPUTE phase — 64 cycles, load_wt=0, act_re=1\n");
    int act_re_errors = 0;
    for (int t = 0; t < COMP_CYC; t++) {
        if (dut->load_wt != 0) {
            printf("  FAIL: load_wt=1 during compute cycle %d\n", t);
            ++g_errors;
        }
        bool re_ok = (dut->act_re_0 == 1);  // act_buf_sel=0 → act_re_0
        if (!re_ok && act_re_errors < 2) {
            printf("  FAIL: act_re_0=0 during compute cycle %d\n", t);
            ++g_errors; ++act_re_errors;
        }
        act_sram_tick(0, (bool)dut->act_re_0);
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }

    // ── Test 4: CAPTURE — out_we fires once, out_wdata = psum_out ─────────
    printf("Test 4: CAPTURE — out_we fires, out_wdata mirrors psum_out\n");
    // At this point state should be CAPTURE on the current clock edge
    if (dut->out_we != 1) {
        printf("  FAIL: out_we not asserted in CAPTURE cycle\n");
        ++g_errors;
    }
    char label[48];
    for (int c = 0; c < N; c++) {
        uint32_t word = dut->out_wdata[c];   // Verilator unpacks 1024-bit as uint32_t[32]
        snprintf(label, sizeof(label), "out_wdata[%d]", c);
        chk(label, word, (uint32_t)(c + 1));
    }
    tick(dut);

    // ── Test 5: DONE pulse and return to IDLE ─────────────────────────────
    printf("Test 5: DONE pulse and return to IDLE\n");
    chk("done",    (uint32_t)dut->done,   1);
    chk("out_we after capture", (uint32_t)dut->out_we, 0);
    tick(dut);
    chk("idle after done", (uint32_t)dut->done, 0);
    chk("load_wt idle",    (uint32_t)dut->load_wt, 0);

    // ── Test 6: two-tile accumulation ─────────────────────────────────────
    // Tile 1: first_tile=1, last_tile=0 → out_we must NOT fire; accum_reg
    //         is loaded with psum_out[c]=c+1 during CAPTURE.
    // Tile 2: first_tile=0, last_tile=1 → psum_in[c] must equal c+1 from
    //         accum_reg; out_we must fire.
    printf("Test 6: Two-tile accumulation\n");

    dut->first_tile = 1; dut->last_tile = 0;
    dut->start = 1; tick(dut); dut->start = 0;

    for (int t = 0; t < LOAD_CYC; t++) {
        wt_sram_tick((uint32_t)dut->wt_addr, nullptr, false);
        drive_wt_rdata(dut);
        act_sram_tick(0, (bool)(dut->act_re_0 || dut->act_re_1));
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }
    for (int t = 0; t < COMP_CYC; t++) {
        act_sram_tick(0, (bool)dut->act_re_0);
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }
    // CAPTURE cycle for tile 1 — out_we must be 0 (last_tile=0)
    if (dut->out_we != 0) {
        printf("  FAIL: out_we asserted on tile 1 CAPTURE (last_tile=0)\n");
        ++g_errors;
    }
    tick(dut); // DONE_ST
    tick(dut); // back to IDLE

    // Tile 2 — first_tile=0, last_tile=1
    dut->first_tile = 0; dut->last_tile = 1;
    dut->start = 1; tick(dut); dut->start = 0;

    for (int t = 0; t < LOAD_CYC; t++) {
        wt_sram_tick((uint32_t)dut->wt_addr, nullptr, false);
        drive_wt_rdata(dut);
        act_sram_tick(0, (bool)(dut->act_re_0 || dut->act_re_1));
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }
    // Verify psum_in loaded from accum_reg at start of COMPUTE
    {
        bool psum_in_ok = true;
        for (int c = 0; c < N; c++) {
            if (dut->psum_in[c] != (uint32_t)(c + 1)) {
                snprintf(label, sizeof(label), "psum_in[%d] tile2", c);
                chk(label, dut->psum_in[c], (uint32_t)(c + 1));
                psum_in_ok = false;
            }
        }
        if (psum_in_ok) printf("  psum_in loaded from accum_reg\n");
    }
    for (int t = 0; t < COMP_CYC; t++) {
        act_sram_tick(0, (bool)dut->act_re_0);
        drive_act_rdata(dut);
        drive_psum_out(dut);
        tick(dut);
    }
    // CAPTURE cycle for tile 2 — out_we must be 1 (last_tile=1)
    if (dut->out_we != 1) {
        printf("  FAIL: out_we not asserted on tile 2 CAPTURE (last_tile=1)\n");
        ++g_errors;
    }
    tick(dut); tick(dut); // DONE_ST + IDLE

    if (g_errors == 0) printf("All controller tests PASSED.\n");
    else               printf("%d controller test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut; delete ctx;
    return g_errors ? 1 : 0;
}
