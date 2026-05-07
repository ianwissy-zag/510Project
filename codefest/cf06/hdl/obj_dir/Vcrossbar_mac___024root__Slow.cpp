// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vcrossbar_mac.h for the primary calling header

#include "Vcrossbar_mac__pch.h"
#include "Vcrossbar_mac__Syms.h"
#include "Vcrossbar_mac___024root.h"

void Vcrossbar_mac___024root___ctor_var_reset(Vcrossbar_mac___024root* vlSelf);

Vcrossbar_mac___024root::Vcrossbar_mac___024root(Vcrossbar_mac__Syms* symsp, const char* v__name)
    : VerilatedModule{v__name}
    , __VdlySched{*symsp->_vm_contextp__}
    , vlSymsp{symsp}
 {
    // Reset structure values
    Vcrossbar_mac___024root___ctor_var_reset(this);
}

void Vcrossbar_mac___024root::__Vconfigure(bool first) {
    if (false && first) {}  // Prevent unused
}

Vcrossbar_mac___024root::~Vcrossbar_mac___024root() {
}
