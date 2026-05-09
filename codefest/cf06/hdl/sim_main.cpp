#include "Vcrossbar_tb.h"
#include "verilated.h"

int main(int argc, char* argv[]) {
    VerilatedContext* const ctx = new VerilatedContext;
    ctx->commandArgs(argc, argv);
    Vcrossbar_tb* const top = new Vcrossbar_tb{ctx};
    while (!ctx->gotFinish()) {
        ctx->timeInc(1);
        top->eval();
    }
    top->final();
    delete top;
    delete ctx;
    return 0;
}
