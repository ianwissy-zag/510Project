// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vcrossbar_mac__pch.h"

//============================================================
// Constructors

Vcrossbar_mac::Vcrossbar_mac(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vcrossbar_mac__Syms(contextp(), _vcname__, this)}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vcrossbar_mac::Vcrossbar_mac(const char* _vcname__)
    : Vcrossbar_mac(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vcrossbar_mac::~Vcrossbar_mac() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vcrossbar_mac___024root___eval_debug_assertions(Vcrossbar_mac___024root* vlSelf);
#endif  // VL_DEBUG
void Vcrossbar_mac___024root___eval_static(Vcrossbar_mac___024root* vlSelf);
void Vcrossbar_mac___024root___eval_initial(Vcrossbar_mac___024root* vlSelf);
void Vcrossbar_mac___024root___eval_settle(Vcrossbar_mac___024root* vlSelf);
void Vcrossbar_mac___024root___eval(Vcrossbar_mac___024root* vlSelf);

void Vcrossbar_mac::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vcrossbar_mac::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vcrossbar_mac___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        vlSymsp->__Vm_didInit = true;
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vcrossbar_mac___024root___eval_static(&(vlSymsp->TOP));
        Vcrossbar_mac___024root___eval_initial(&(vlSymsp->TOP));
        Vcrossbar_mac___024root___eval_settle(&(vlSymsp->TOP));
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vcrossbar_mac___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

//============================================================
// Events and timing
bool Vcrossbar_mac::eventsPending() { return !vlSymsp->TOP.__VdlySched.empty(); }

uint64_t Vcrossbar_mac::nextTimeSlot() { return vlSymsp->TOP.__VdlySched.nextTimeSlot(); }

//============================================================
// Utilities

const char* Vcrossbar_mac::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vcrossbar_mac___024root___eval_final(Vcrossbar_mac___024root* vlSelf);

VL_ATTR_COLD void Vcrossbar_mac::final() {
    Vcrossbar_mac___024root___eval_final(&(vlSymsp->TOP));
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vcrossbar_mac::hierName() const { return vlSymsp->name(); }
const char* Vcrossbar_mac::modelName() const { return "Vcrossbar_mac"; }
unsigned Vcrossbar_mac::threads() const { return 1; }
void Vcrossbar_mac::prepareClone() const { contextp()->prepareClone(); }
void Vcrossbar_mac::atClone() const {
    contextp()->threadPoolpOnClone();
}

//============================================================
// Trace configuration

VL_ATTR_COLD void Vcrossbar_mac::trace(VerilatedVcdC* tfp, int levels, int options) {
    vl_fatal(__FILE__, __LINE__, __FILE__,"'Vcrossbar_mac::trace()' called on model that was Verilated without --trace option");
}
