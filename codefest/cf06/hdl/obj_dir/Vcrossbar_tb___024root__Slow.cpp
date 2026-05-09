// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vcrossbar_tb.h for the primary calling header

#include "Vcrossbar_tb__pch.h"
#include "Vcrossbar_tb__Syms.h"
#include "Vcrossbar_tb___024root.h"

void Vcrossbar_tb___024root___ctor_var_reset(Vcrossbar_tb___024root* vlSelf);

Vcrossbar_tb___024root::Vcrossbar_tb___024root(Vcrossbar_tb__Syms* symsp, const char* v__name)
    : VerilatedModule{v__name}
    , __VdlySched{*symsp->_vm_contextp__}
    , vlSymsp{symsp}
 {
    // Reset structure values
    Vcrossbar_tb___024root___ctor_var_reset(this);
}

void Vcrossbar_tb___024root::__Vconfigure(bool first) {
    if (false && first) {}  // Prevent unused
}

Vcrossbar_tb___024root::~Vcrossbar_tb___024root() {
}
