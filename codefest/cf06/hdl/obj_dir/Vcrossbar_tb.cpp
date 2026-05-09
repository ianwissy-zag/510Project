// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vcrossbar_tb__pch.h"

//============================================================
// Constructors

Vcrossbar_tb::Vcrossbar_tb(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vcrossbar_tb__Syms(contextp(), _vcname__, this)}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vcrossbar_tb::Vcrossbar_tb(const char* _vcname__)
    : Vcrossbar_tb(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vcrossbar_tb::~Vcrossbar_tb() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vcrossbar_tb___024root___eval_debug_assertions(Vcrossbar_tb___024root* vlSelf);
#endif  // VL_DEBUG
void Vcrossbar_tb___024root___eval_static(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval_initial(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval_settle(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval(Vcrossbar_tb___024root* vlSelf);

void Vcrossbar_tb::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vcrossbar_tb::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vcrossbar_tb___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        vlSymsp->__Vm_didInit = true;
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vcrossbar_tb___024root___eval_static(&(vlSymsp->TOP));
        Vcrossbar_tb___024root___eval_initial(&(vlSymsp->TOP));
        Vcrossbar_tb___024root___eval_settle(&(vlSymsp->TOP));
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vcrossbar_tb___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vcrossbar_tb::eventsPending() { return !vlSymsp->TOP.__VdlySched.empty(); }

uint64_t Vcrossbar_tb::nextTimeSlot() { return vlSymsp->TOP.__VdlySched.nextTimeSlot(); }

//============================================================
// Utilities

const char* Vcrossbar_tb::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vcrossbar_tb___024root___eval_final(Vcrossbar_tb___024root* vlSelf);

VL_ATTR_COLD void Vcrossbar_tb::final() {
    Vcrossbar_tb___024root___eval_final(&(vlSymsp->TOP));
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vcrossbar_tb::hierName() const { return vlSymsp->name(); }
const char* Vcrossbar_tb::modelName() const { return "Vcrossbar_tb"; }
unsigned Vcrossbar_tb::threads() const { return 1; }
void Vcrossbar_tb::prepareClone() const { contextp()->prepareClone(); }
void Vcrossbar_tb::atClone() const {
    contextp()->threadPoolpOnClone();
}

//============================================================
// Trace configuration

VL_ATTR_COLD void Vcrossbar_tb::trace(VerilatedVcdC* tfp, int levels, int options) {
    vl_fatal(__FILE__, __LINE__, __FILE__,"'Vcrossbar_tb::trace()' called on model that was Verilated without --trace option");
}
