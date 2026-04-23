// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb___024unit.h"

VL_ATTR_COLD void Vmac_tb___024unit___ctor_var_reset(Vmac_tb___024unit* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+  Vmac_tb___024unit___ctor_var_reset\n"); );
    // Body
    vlSelf->__VmonitorOff = VL_RAND_RESET_I(1);
}
