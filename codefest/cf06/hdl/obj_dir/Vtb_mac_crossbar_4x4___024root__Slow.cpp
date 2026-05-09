// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vtb_mac_crossbar_4x4.h for the primary calling header

#include "Vtb_mac_crossbar_4x4__pch.h"
#include "Vtb_mac_crossbar_4x4__Syms.h"
#include "Vtb_mac_crossbar_4x4___024root.h"

void Vtb_mac_crossbar_4x4___024root___ctor_var_reset(Vtb_mac_crossbar_4x4___024root* vlSelf);

Vtb_mac_crossbar_4x4___024root::Vtb_mac_crossbar_4x4___024root(Vtb_mac_crossbar_4x4__Syms* symsp, const char* v__name)
    : VerilatedModule{v__name}
    , __VdlySched{*symsp->_vm_contextp__}
    , vlSymsp{symsp}
 {
    // Reset structure values
    Vtb_mac_crossbar_4x4___024root___ctor_var_reset(this);
}

void Vtb_mac_crossbar_4x4___024root::__Vconfigure(bool first) {
    if (false && first) {}  // Prevent unused
}

Vtb_mac_crossbar_4x4___024root::~Vtb_mac_crossbar_4x4___024root() {
}
