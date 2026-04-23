// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb__Syms.h"
#include "Vmac_tb___024root.h"

void Vmac_tb___024root___ctor_var_reset(Vmac_tb___024root* vlSelf);

Vmac_tb___024root::Vmac_tb___024root(Vmac_tb__Syms* symsp, const char* v__name)
    : VerilatedModule{v__name}
    , __VdlySched{*symsp->_vm_contextp__}
    , vlSymsp{symsp}
 {
    // Reset structure values
    Vmac_tb___024root___ctor_var_reset(this);
}

void Vmac_tb___024root::__Vconfigure(bool first) {
    if (false && first) {}  // Prevent unused
}

Vmac_tb___024root::~Vmac_tb___024root() {
}
