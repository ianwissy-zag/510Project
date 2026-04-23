// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb__Syms.h"
#include "Vmac_tb___024root.h"

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__act(Vmac_tb___024root* vlSelf);
#endif  // VL_DEBUG

void Vmac_tb___024root___eval_triggers__act(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_triggers__act\n"); );
    // Body
    vlSelf->__VactTriggered.set(0U, ((IData)(vlSelf->tb_mac__DOT__clk) 
                                     & (~ (IData)(vlSelf->__Vtrigprevexpr___TOP__tb_mac__DOT__clk__0))));
    vlSelf->__VactTriggered.set(1U, ((~ (IData)(vlSelf->tb_mac__DOT__clk)) 
                                     & (IData)(vlSelf->__Vtrigprevexpr___TOP__tb_mac__DOT__clk__0)));
    vlSelf->__VactTriggered.set(2U, vlSelf->__VdlySched.awaitingCurrentTime());
    vlSelf->__Vtrigprevexpr___TOP__tb_mac__DOT__clk__0 
        = vlSelf->tb_mac__DOT__clk;
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vmac_tb___024root___dump_triggers__act(vlSelf);
    }
#endif
}
