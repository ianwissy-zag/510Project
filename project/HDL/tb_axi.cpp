// Verilator testbench for axis_to_pingpong_buffer (tuser-based routing)
#include "Vaxis_to_pingpong_buffer.h"
#include "verilated.h"
#include <cstdio>

static int g_errors = 0;

static void chk(const char* label, uint32_t got, uint32_t expected) {
    if (got != expected) {
        printf("  FAIL [%s]: got %u, expected %u\n", label, got, expected);
        g_errors++;
    }
}

static void set_tdata(Vaxis_to_pingpong_buffer* dut, uint32_t word0) {
    dut->s_axis_tdata[0] = word0;
    for (int i = 1; i < 16; i++) dut->s_axis_tdata[i] = 0;
}

// Check combinational outputs for one beat.
// Called after setting inputs and eval() but BEFORE the posedge.
// tuser encoding: bit[1]=dest (0=wt,1=act), bit[0]=buf (0=ping,1=pong)
static void check_beat(Vaxis_to_pingpong_buffer* dut,
                       int beat, uint8_t tuser) {
    bool is_wt   = !(tuser & 0x2);
    bool is_ping = !(tuser & 0x1);
    char label[64];

    uint8_t we_p    = is_wt ? dut->wt_we_0  : dut->act_we_0;
    uint8_t we_pp   = is_wt ? dut->wt_we_1  : dut->act_we_1;
    uint8_t we_ot0  = is_wt ? dut->act_we_0 : dut->wt_we_0;
    uint8_t we_ot1  = is_wt ? dut->act_we_1 : dut->wt_we_1;
    uint8_t addr    = is_wt ? dut->wt_addr  : dut->act_addr;

    snprintf(label, sizeof(label), "beat%02d we_ping",   beat); chk(label, we_p,   is_ping ? 1 : 0);
    snprintf(label, sizeof(label), "beat%02d we_pong",   beat); chk(label, we_pp,  is_ping ? 0 : 1);
    snprintf(label, sizeof(label), "beat%02d we_other0", beat); chk(label, we_ot0, 0);
    snprintf(label, sizeof(label), "beat%02d we_other1", beat); chk(label, we_ot1, 0);
    snprintf(label, sizeof(label), "beat%02d addr",      beat); chk(label, addr, (uint32_t)beat);
}

int main(int argc, char** argv) {
    VerilatedContext* ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vaxis_to_pingpong_buffer* dut = new Vaxis_to_pingpong_buffer{ctx};

    // ── Reset ────────────────────────────────────────────────────────────
    dut->clk = 0; dut->rst_n = 0;
    dut->s_axis_tvalid = 0; dut->s_axis_tlast = 0; dut->s_axis_tuser = 0;
    set_tdata(dut, 0); dut->eval();
    for (int i = 0; i < 4; i++) { dut->clk ^= 1; dut->eval(); }
    dut->clk = 0; dut->rst_n = 1; dut->eval();

    // ── Tests 1–4: all four routing modes via tuser ───────────────────────
    // tuser: 0=wt_ping  1=wt_pong  2=act_ping  3=act_pong
    const char* names[4] = {"wt_ping", "wt_pong", "act_ping", "act_pong"};
    for (int mode = 0; mode < 4; mode++) {
        printf("Test %d: Route to %s (tuser=%d)\n", mode+1, names[mode], mode);
        dut->s_axis_tvalid = 0; dut->eval();

        for (int beat = 0; beat < 32; beat++) {
            set_tdata(dut, (uint32_t)beat);
            dut->s_axis_tuser  = (uint8_t)mode;
            dut->s_axis_tvalid = 1;
            dut->s_axis_tlast  = (beat == 31) ? 1 : 0;
            dut->eval();

            check_beat(dut, beat, (uint8_t)mode);

            dut->clk = 1; dut->eval();
            dut->clk = 0; dut->eval();
        }

        dut->s_axis_tvalid = 0;
        dut->clk = 1; dut->eval();
        dut->clk = 0; dut->eval();
    }

    // ── Test 5: tuser latched on beat 0 – mid-packet change is ignored ────
    // Send a wt_ping packet (tuser=0). After beat 0 is captured, change
    // tuser to act_ping (2). Routing must stay as wt_ping for all 32 beats.
    printf("Test 5: tuser mid-packet change ignored (routing locked to beat 0)\n");
    for (int beat = 0; beat < 32; beat++) {
        set_tdata(dut, (uint32_t)beat);
        dut->s_axis_tuser  = (beat == 0) ? 0 : 2; // change to act_ping after beat 0
        dut->s_axis_tvalid = 1;
        dut->s_axis_tlast  = (beat == 31) ? 1 : 0;
        dut->eval();

        // Routing should remain wt_ping (tuser=0) throughout
        check_beat(dut, beat, 0);

        dut->clk = 1; dut->eval();
        dut->clk = 0; dut->eval();
    }
    dut->s_axis_tvalid = 0;
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();

    // ── Test 6: tready deasserted in reset ───────────────────────────────
    printf("Test 6: tready = rst_n\n");
    dut->rst_n = 0; dut->eval();
    chk("tready during reset", dut->s_axis_tready, 0);
    dut->rst_n = 1; dut->eval();
    chk("tready after reset",  dut->s_axis_tready, 1);

    // ── Test 7: no write enables when tvalid is low ───────────────────────
    printf("Test 7: No write enables when tvalid=0\n");
    dut->s_axis_tuser  = 0;
    dut->s_axis_tvalid = 0; dut->eval();
    chk("wt_we_0 idle",  dut->wt_we_0,  0);
    chk("wt_we_1 idle",  dut->wt_we_1,  0);
    chk("act_we_0 idle", dut->act_we_0, 0);
    chk("act_we_1 idle", dut->act_we_1, 0);

    if (g_errors == 0) printf("All AXI tests PASSED.\n");
    else               printf("%d AXI test(s) FAILED.\n", g_errors);

    dut->final();
    delete dut;
    delete ctx;
    return g_errors ? 1 : 0;
}
