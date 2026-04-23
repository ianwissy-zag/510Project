// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb__Syms.h"
#include "Vmac_tb___024unit.h"

void Vmac_tb___024unit___ctor_var_reset(Vmac_tb___024unit* vlSelf);

Vmac_tb___024unit::Vmac_tb___024unit(Vmac_tb__Syms* symsp, const char* v__name)
    : VerilatedModule{v__name}
    , vlSymsp{symsp}
 {
    // Reset structure values
    Vmac_tb___024unit___ctor_var_reset(this);
}

void Vmac_tb___024unit::__Vconfigure(bool first) {
    if (false && first) {}  // Prevent unused
}

Vmac_tb___024unit::~Vmac_tb___024unit() {
}
