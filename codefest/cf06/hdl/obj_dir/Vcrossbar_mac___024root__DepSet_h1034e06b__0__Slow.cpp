// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vcrossbar_mac.h for the primary calling header

#include "Vcrossbar_mac__pch.h"
#include "Vcrossbar_mac___024root.h"

VL_ATTR_COLD void Vcrossbar_mac___024root___eval_static(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___eval_static\n"); );
}

VL_ATTR_COLD void Vcrossbar_mac___024root___eval_final(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___eval_final\n"); );
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vcrossbar_mac___024root___dump_triggers__stl(Vcrossbar_mac___024root* vlSelf);
#endif  // VL_DEBUG
VL_ATTR_COLD bool Vcrossbar_mac___024root___eval_phase__stl(Vcrossbar_mac___024root* vlSelf);

VL_ATTR_COLD void Vcrossbar_mac___024root___eval_settle(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___eval_settle\n"); );
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
            Vcrossbar_mac___024root___dump_triggers__stl(vlSelf);
#endif
            VL_FATAL_MT("crossbar_tb.sv", 3, "", "Settle region did not converge.");
        }
        __VstlIterCount = ((IData)(1U) + __VstlIterCount);
        __VstlContinue = 0U;
        if (Vcrossbar_mac___024root___eval_phase__stl(vlSelf)) {
            __VstlContinue = 1U;
        }
        vlSelf->__VstlFirstIteration = 0U;
    }
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vcrossbar_mac___024root___dump_triggers__stl(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___dump_triggers__stl\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VstlTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        VL_DBG_MSGF("         'stl' region trigger index 0 is active: Internal 'stl' trigger - first iteration\n");
    }
}
#endif  // VL_DEBUG

void Vcrossbar_mac___024root___act_sequent__TOP__0(Vcrossbar_mac___024root* vlSelf);

VL_ATTR_COLD void Vcrossbar_mac___024root___eval_stl(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___eval_stl\n"); );
    // Body
    if ((1ULL & vlSelf->__VstlTriggered.word(0U))) {
        Vcrossbar_mac___024root___act_sequent__TOP__0(vlSelf);
    }
}

VL_ATTR_COLD void Vcrossbar_mac___024root___eval_triggers__stl(Vcrossbar_mac___024root* vlSelf);

VL_ATTR_COLD bool Vcrossbar_mac___024root___eval_phase__stl(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___eval_phase__stl\n"); );
    // Init
    CData/*0:0*/ __VstlExecute;
    // Body
    Vcrossbar_mac___024root___eval_triggers__stl(vlSelf);
    __VstlExecute = vlSelf->__VstlTriggered.any();
    if (__VstlExecute) {
        Vcrossbar_mac___024root___eval_stl(vlSelf);
    }
    return (__VstlExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vcrossbar_mac___024root___dump_triggers__act(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___dump_triggers__act\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VactTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VactTriggered.word(0U))) {
        VL_DBG_MSGF("         'act' region trigger index 0 is active: @([true] __VdlySched.awaitingCurrentTime())\n");
    }
}
#endif  // VL_DEBUG

#ifdef VL_DEBUG
VL_ATTR_COLD void Vcrossbar_mac___024root___dump_triggers__nba(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___dump_triggers__nba\n"); );
    // Body
    if ((1U & (~ (IData)(vlSelf->__VnbaTriggered.any())))) {
        VL_DBG_MSGF("         No triggers active\n");
    }
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        VL_DBG_MSGF("         'nba' region trigger index 0 is active: @([true] __VdlySched.awaitingCurrentTime())\n");
    }
}
#endif  // VL_DEBUG

VL_ATTR_COLD void Vcrossbar_mac___024root___ctor_var_reset(Vcrossbar_mac___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_mac__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_mac___024root___ctor_var_reset\n"); );
    // Body
    for (int __Vi0 = 0; __Vi0 < 4; ++__Vi0) {
        vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[__Vi0] = VL_RAND_RESET_I(8);
    }
    vlSelf->tb_mac_crossbar_4x4__DOT__weights_i = VL_RAND_RESET_I(16);
    for (int __Vi0 = 0; __Vi0 < 4; ++__Vi0) {
        vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o[__Vi0] = VL_RAND_RESET_I(10);
    }
    for (int __Vi0 = 0; __Vi0 < 4; ++__Vi0) {
        for (int __Vi1 = 0; __Vi1 < 4; ++__Vi1) {
            vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[__Vi0][__Vi1] = VL_RAND_RESET_I(9);
        }
    }
    }
