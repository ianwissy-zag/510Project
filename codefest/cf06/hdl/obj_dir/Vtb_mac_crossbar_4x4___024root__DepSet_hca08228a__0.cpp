// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vtb_mac_crossbar_4x4.h for the primary calling header

#include "Vtb_mac_crossbar_4x4__pch.h"
#include "Vtb_mac_crossbar_4x4___024root.h"

VlCoroutine Vtb_mac_crossbar_4x4___024root___eval_initial__TOP__Vtiming__0(Vtb_mac_crossbar_4x4___024root* vlSelf);

void Vtb_mac_crossbar_4x4___024root___eval_initial(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_initial\n"); );
    // Body
    vlSelf->__Vm_traceActivity[1U] = 1U;
    Vtb_mac_crossbar_4x4___024root___eval_initial__TOP__Vtiming__0(vlSelf);
}

VL_INLINE_OPT void Vtb_mac_crossbar_4x4___024root___act_sequent__TOP__0(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___act_sequent__TOP__0\n"); );
    // Body
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[0U][0U] 
        = (0x1ffU & ((1U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [0U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [0U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[1U][0U] 
        = (0x1ffU & ((0x10U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [1U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [1U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[2U][0U] 
        = (0x1ffU & ((0x100U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [2U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [2U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[3U][0U] 
        = (0x1ffU & ((0x1000U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [3U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [3U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[0U][1U] 
        = (0x1ffU & ((2U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [0U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [0U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[1U][1U] 
        = (0x1ffU & ((0x20U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [1U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [1U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[2U][1U] 
        = (0x1ffU & ((0x200U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [2U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [2U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[3U][1U] 
        = (0x1ffU & ((0x2000U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [3U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [3U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[0U][2U] 
        = (0x1ffU & ((4U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [0U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [0U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[1U][2U] 
        = (0x1ffU & ((0x40U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [1U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [1U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[2U][2U] 
        = (0x1ffU & ((0x400U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [2U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [2U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[3U][2U] 
        = (0x1ffU & ((0x4000U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [3U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [3U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[0U][3U] 
        = (0x1ffU & ((8U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [0U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [0U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[1U][3U] 
        = (0x1ffU & ((0x80U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [1U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [1U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[2U][3U] 
        = (0x1ffU & ((0x800U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [2U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [2U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results[3U][3U] 
        = (0x1ffU & ((0x8000U & (IData)(vlSelf->tb_mac_crossbar_4x4__DOT__weights_i))
                      ? (- VL_EXTENDS_II(9,8, vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                         [3U])) : VL_EXTENDS_II(9,8, 
                                                                vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
                                                                [3U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o[0U] 
        = (0x3ffU & (((VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                     [0U][0U]) + VL_EXTENDS_II(10,9, 
                                                               vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                                               [1U]
                                                               [0U])) 
                      + VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                      [2U][0U])) + 
                     VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                   [3U][0U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o[1U] 
        = (0x3ffU & (((VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                     [0U][1U]) + VL_EXTENDS_II(10,9, 
                                                               vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                                               [1U]
                                                               [1U])) 
                      + VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                      [2U][1U])) + 
                     VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                   [3U][1U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o[2U] 
        = (0x3ffU & (((VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                     [0U][2U]) + VL_EXTENDS_II(10,9, 
                                                               vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                                               [1U]
                                                               [2U])) 
                      + VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                      [2U][2U])) + 
                     VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                   [3U][2U])));
    vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o[3U] 
        = (0x3ffU & (((VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                     [0U][3U]) + VL_EXTENDS_II(10,9, 
                                                               vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                                               [1U]
                                                               [3U])) 
                      + VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                      [2U][3U])) + 
                     VL_EXTENDS_II(10,9, vlSelf->tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results
                                   [3U][3U])));
}

void Vtb_mac_crossbar_4x4___024root___eval_act(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_act\n"); );
    // Body
    if ((1ULL & vlSelf->__VactTriggered.word(0U))) {
        Vtb_mac_crossbar_4x4___024root___act_sequent__TOP__0(vlSelf);
        vlSelf->__Vm_traceActivity[3U] = 1U;
    }
}

void Vtb_mac_crossbar_4x4___024root___eval_nba(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_nba\n"); );
    // Body
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vtb_mac_crossbar_4x4___024root___act_sequent__TOP__0(vlSelf);
        vlSelf->__Vm_traceActivity[4U] = 1U;
    }
}

void Vtb_mac_crossbar_4x4___024root___timing_resume(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___timing_resume\n"); );
    // Body
    if ((1ULL & vlSelf->__VactTriggered.word(0U))) {
        vlSelf->__VdlySched.resume();
    }
}

void Vtb_mac_crossbar_4x4___024root___eval_triggers__act(Vtb_mac_crossbar_4x4___024root* vlSelf);

bool Vtb_mac_crossbar_4x4___024root___eval_phase__act(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_phase__act\n"); );
    // Init
    VlTriggerVec<1> __VpreTriggered;
    CData/*0:0*/ __VactExecute;
    // Body
    Vtb_mac_crossbar_4x4___024root___eval_triggers__act(vlSelf);
    __VactExecute = vlSelf->__VactTriggered.any();
    if (__VactExecute) {
        __VpreTriggered.andNot(vlSelf->__VactTriggered, vlSelf->__VnbaTriggered);
        vlSelf->__VnbaTriggered.thisOr(vlSelf->__VactTriggered);
        Vtb_mac_crossbar_4x4___024root___timing_resume(vlSelf);
        Vtb_mac_crossbar_4x4___024root___eval_act(vlSelf);
    }
    return (__VactExecute);
}

bool Vtb_mac_crossbar_4x4___024root___eval_phase__nba(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_phase__nba\n"); );
    // Init
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = vlSelf->__VnbaTriggered.any();
    if (__VnbaExecute) {
        Vtb_mac_crossbar_4x4___024root___eval_nba(vlSelf);
        vlSelf->__VnbaTriggered.clear();
    }
    return (__VnbaExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vtb_mac_crossbar_4x4___024root___dump_triggers__nba(Vtb_mac_crossbar_4x4___024root* vlSelf);
#endif  // VL_DEBUG
#ifdef VL_DEBUG
VL_ATTR_COLD void Vtb_mac_crossbar_4x4___024root___dump_triggers__act(Vtb_mac_crossbar_4x4___024root* vlSelf);
#endif  // VL_DEBUG

void Vtb_mac_crossbar_4x4___024root___eval(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval\n"); );
    // Init
    IData/*31:0*/ __VnbaIterCount;
    CData/*0:0*/ __VnbaContinue;
    // Body
    __VnbaIterCount = 0U;
    __VnbaContinue = 1U;
    while (__VnbaContinue) {
        if (VL_UNLIKELY((0x64U < __VnbaIterCount))) {
#ifdef VL_DEBUG
            Vtb_mac_crossbar_4x4___024root___dump_triggers__nba(vlSelf);
#endif
            VL_FATAL_MT("crossbar_tb.sv", 3, "", "NBA region did not converge.");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        __VnbaContinue = 0U;
        vlSelf->__VactIterCount = 0U;
        vlSelf->__VactContinue = 1U;
        while (vlSelf->__VactContinue) {
            if (VL_UNLIKELY((0x64U < vlSelf->__VactIterCount))) {
#ifdef VL_DEBUG
                Vtb_mac_crossbar_4x4___024root___dump_triggers__act(vlSelf);
#endif
                VL_FATAL_MT("crossbar_tb.sv", 3, "", "Active region did not converge.");
            }
            vlSelf->__VactIterCount = ((IData)(1U) 
                                       + vlSelf->__VactIterCount);
            vlSelf->__VactContinue = 0U;
            if (Vtb_mac_crossbar_4x4___024root___eval_phase__act(vlSelf)) {
                vlSelf->__VactContinue = 1U;
            }
        }
        if (Vtb_mac_crossbar_4x4___024root___eval_phase__nba(vlSelf)) {
            __VnbaContinue = 1U;
        }
    }
}

#ifdef VL_DEBUG
void Vtb_mac_crossbar_4x4___024root___eval_debug_assertions(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_debug_assertions\n"); );
}
#endif  // VL_DEBUG
