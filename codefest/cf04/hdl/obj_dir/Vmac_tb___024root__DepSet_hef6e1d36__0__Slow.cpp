// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb___024root.h"

VL_ATTR_COLD void Vmac_tb___024root___eval_static__TOP(Vmac_tb___024root* vlSelf);

VL_ATTR_COLD void Vmac_tb___024root___eval_static(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_static\n"); );
    // Body
    Vmac_tb___024root___eval_static__TOP(vlSelf);
}

VL_ATTR_COLD void Vmac_tb___024root___eval_static__TOP(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_static__TOP\n"); );
    // Body
    vlSelf->tb_mac__DOT__error_count = 0U;
}

VL_ATTR_COLD void Vmac_tb___024root___eval_final(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_final\n"); );
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__stl(Vmac_tb___024root* vlSelf);
#endif  // VL_DEBUG
VL_ATTR_COLD bool Vmac_tb___024root___eval_phase__stl(Vmac_tb___024root* vlSelf);

VL_ATTR_COLD void Vmac_tb___024root___eval_settle(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_settle\n"); );
    // Init
    IData/*31:0*/ __VstlIterCount;
    CData/*0:0*/ __VstlContinue;
    // Body
    __VstlIterCount = 0U;
    vlSelf->__VstlFirstIteration = 1U;
    __VstlContinue = 1U;
    while (__VstlContinue) {
        if (VL_UNLIKELY((0x64U < __VstlIterCount))) {
#ifdef VL_DEBUG
            Vmac_tb___024root___dump_triggers__stl(vlSelf);
#endif
            VL_FATAL_MT("mac_tb.v", 3, "", "Settle region did not converge.");
        }
        __VstlIterCount = ((IData)(1U) + __VstlIterCount);
        __VstlContinue = 0U;
        if (Vmac_tb___024root___eval_phase__stl(vlSelf)) {
            __VstlContinue = 1U;
        }
        vlSelf->__VstlFirstIteration = 0U;
    }
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__stl(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___dump_triggers__stl\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VstlTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        VL_DBG_MSGF("         'stl' region trigger index 0 is active: Internal 'stl' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

void Vmac_tb___024root___act_sequent__TOP__0(Vmac_tb___024root* vlSelf);

VL_ATTR_COLD void Vmac_tb___024root___eval_stl(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_stl\n"); );
    // Body
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        Vmac_tb___024root___act_sequent__TOP__0(vlSelf);
    }
}

VL_ATTR_COLD void Vmac_tb___024root___eval_triggers__stl(Vmac_tb___024root* vlSelf);

VL_ATTR_COLD bool Vmac_tb___024root___eval_phase__stl(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_phase__stl\n"); );
    // Init
    CData/*0:0*/ __VstlExecute;
    // Body
    Vmac_tb___024root___eval_triggers__stl(vlSelf);
    __VstlExecute = vlSelf->__VstlTriggered.any();
    if (__VstlExecute) {
        Vmac_tb___024root___eval_stl(vlSelf);
    }
    return (__VstlExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__act(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VactTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 0 is active: @(posedge tb_mac.clk)\n");
    }
    if ((2ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 1 is active: @(negedge tb_mac.clk)\n");
    }
    if ((4ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 2 is active: @([true] __VdlySched.awaitingCurrentTime())\n");
    }
}
#endif  // VL_DEBUG

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__nba(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___dump_triggers__nba\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VnbaTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 0 is active: @(posedge tb_mac.clk)\n");
    }
    if ((2ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 1 is active: @(negedge tb_mac.clk)\n");
    }
    if ((4ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 2 is active: @([true] __VdlySched.awaitingCurrentTime())\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vmac_tb___024root___ctor_var_reset(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___ctor_var_reset\n"); );
    // Body
    vlSelf->tb_mac__DOT__clk = VL_RAND_RESET_I(1);
    vlSelf->tb_mac__DOT__rst = VL_RAND_RESET_I(1);
    vlSelf->tb_mac__DOT__a = VL_RAND_RESET_I(8);
    vlSelf->tb_mac__DOT__b = VL_RAND_RESET_I(8);
    vlSelf->tb_mac__DOT__out = VL_RAND_RESET_I(32);
    vlSelf->tb_mac__DOT__error_count = VL_RAND_RESET_I(32);
    vlSelf->tb_mac__DOT__uut__DOT__product = VL_RAND_RESET_I(16);
    vlSelf->__Vtrigprevexpr___TOP__tb_mac__DOT__clk__0 = VL_RAND_RESET_I(1);
}
