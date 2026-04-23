// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmac_tb.h for the primary calling header

#include "Vmac_tb__pch.h"
#include "Vmac_tb___024root.h"

VlCoroutine Vmac_tb___024root___eval_initial__TOP__Vtiming__0(Vmac_tb___024root* vlSelf);
VlCoroutine Vmac_tb___024root___eval_initial__TOP__Vtiming__1(Vmac_tb___024root* vlSelf);

void Vmac_tb___024root___eval_initial(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_initial\n"); );
    // Body
    Vmac_tb___024root___eval_initial__TOP__Vtiming__0(vlSelf);
    Vmac_tb___024root___eval_initial__TOP__Vtiming__1(vlSelf);
    vlSelf->__Vtrigprevexpr___TOP__tb_mac__DOT__clk__0 
        = vlSelf->tb_mac__DOT__clk;
}

VL_INLINE_OPT VlCoroutine Vmac_tb___024root___eval_initial__TOP__Vtiming__0(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_initial__TOP__Vtiming__0\n"); );
    // Init
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__0__expected_val;
    __Vtask_tb_mac__DOT__check_output__0__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__1__expected_val;
    __Vtask_tb_mac__DOT__check_output__1__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__2__expected_val;
    __Vtask_tb_mac__DOT__check_output__2__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__3__expected_val;
    __Vtask_tb_mac__DOT__check_output__3__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__4__expected_val;
    __Vtask_tb_mac__DOT__check_output__4__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__5__expected_val;
    __Vtask_tb_mac__DOT__check_output__5__expected_val = 0;
    IData/*31:0*/ __Vtask_tb_mac__DOT__check_output__6__expected_val;
    __Vtask_tb_mac__DOT__check_output__6__expected_val = 0;
    // Body
    vlSelf->tb_mac__DOT__clk = 0U;
    vlSelf->tb_mac__DOT__rst = 1U;
    vlSelf->tb_mac__DOT__a = 0U;
    vlSelf->tb_mac__DOT__b = 0U;
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       48);
    __Vtask_tb_mac__DOT__check_output__0__expected_val = 0U;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__0__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__0__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    vlSelf->tb_mac__DOT__rst = 0U;
    vlSelf->tb_mac__DOT__a = 3U;
    vlSelf->tb_mac__DOT__b = 4U;
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       55);
    __Vtask_tb_mac__DOT__check_output__1__expected_val = 0xcU;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__1__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__1__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       56);
    __Vtask_tb_mac__DOT__check_output__2__expected_val = 0x18U;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__2__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__2__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       57);
    __Vtask_tb_mac__DOT__check_output__3__expected_val = 0x24U;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__3__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__3__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    vlSelf->tb_mac__DOT__rst = 1U;
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       61);
    __Vtask_tb_mac__DOT__check_output__4__expected_val = 0U;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__4__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__4__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    vlSelf->tb_mac__DOT__rst = 0U;
    vlSelf->tb_mac__DOT__a = 0xfbU;
    vlSelf->tb_mac__DOT__b = 2U;
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       67);
    __Vtask_tb_mac__DOT__check_output__5__expected_val = 0xfffffff6U;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__5__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__5__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    co_await vlSelf->__VtrigSched_he644037f__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(negedge tb_mac.clk)", 
                                                       "mac_tb.v", 
                                                       68);
    __Vtask_tb_mac__DOT__check_output__6__expected_val = 0xffffffecU;
    if ((vlSelf->tb_mac__DOT__out != __Vtask_tb_mac__DOT__check_output__6__expected_val)) {
        VL_WRITEF("ERROR at %0t: Expected %4d, but got %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,__Vtask_tb_mac__DOT__check_output__6__expected_val,
                  32,vlSelf->tb_mac__DOT__out);
        vlSelf->tb_mac__DOT__error_count = ((IData)(1U) 
                                            + vlSelf->tb_mac__DOT__error_count);
    } else {
        VL_WRITEF("PASS  at %0t: Output is correctly %4d\n",
                  64,VL_TIME_UNITED_Q(1000),-9,32,vlSelf->tb_mac__DOT__out);
    }
    VL_WRITEF("--------------------------------------------------\n");
    if ((0U == vlSelf->tb_mac__DOT__error_count)) {
        VL_WRITEF("SIMULATION PASSED! All outputs matched expected values.\n");
    } else {
        VL_WRITEF("SIMULATION FAILED with %0d errors.\n",
                  32,vlSelf->tb_mac__DOT__error_count);
    }
    VL_WRITEF("--------------------------------------------------\n");
    VL_FINISH_MT("mac_tb.v", 80, "");
}

VL_INLINE_OPT VlCoroutine Vmac_tb___024root___eval_initial__TOP__Vtiming__1(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_initial__TOP__Vtiming__1\n"); );
    // Body
    while (1U) {
        co_await vlSelf->__VdlySched.delay(0x1388ULL, 
                                           nullptr, 
                                           "mac_tb.v", 
                                           25);
        vlSelf->tb_mac__DOT__clk = (1U & (~ (IData)(vlSelf->tb_mac__DOT__clk)));
    }
}

VL_INLINE_OPT void Vmac_tb___024root___act_sequent__TOP__0(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___act_sequent__TOP__0\n"); );
    // Body
    vlSelf->tb_mac__DOT__uut__DOT__product = (0xffffU 
                                              & VL_MULS_III(16, 
                                                            (0xffffU 
                                                             & VL_EXTENDS_II(16,8, (IData)(vlSelf->tb_mac__DOT__a))), 
                                                            (0xffffU 
                                                             & VL_EXTENDS_II(16,8, (IData)(vlSelf->tb_mac__DOT__b)))));
}

void Vmac_tb___024root___eval_act(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_act\n"); );
    // Body
    if ((2ULL & vlSelf->__VactTriggered.word(0U))) {
        Vmac_tb___024root___act_sequent__TOP__0(vlSelf);
    }
}

VL_INLINE_OPT void Vmac_tb___024root___nba_sequent__TOP__0(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___nba_sequent__TOP__0\n"); );
    // Body
    vlSelf->tb_mac__DOT__out = ((IData)(vlSelf->tb_mac__DOT__rst)
                                 ? 0U : (vlSelf->tb_mac__DOT__out 
                                         + (((- (IData)(
                                                        (1U 
                                                         & ((IData)(vlSelf->tb_mac__DOT__uut__DOT__product) 
                                                            >> 0xfU)))) 
                                             << 0x10U) 
                                            | (IData)(vlSelf->tb_mac__DOT__uut__DOT__product))));
}

void Vmac_tb___024root___eval_nba(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_nba\n"); );
    // Body
    if ((1ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vmac_tb___024root___nba_sequent__TOP__0(vlSelf);
    }
    if ((2ULL & vlSelf->__VnbaTriggered.word(0U))) {
        Vmac_tb___024root___act_sequent__TOP__0(vlSelf);
    }
}

void Vmac_tb___024root___timing_resume(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___timing_resume\n"); );
    // Body
    if ((2ULL & vlSelf->__VactTriggered.word(0U))) {
        vlSelf->__VtrigSched_he644037f__0.resume("@(negedge tb_mac.clk)");
    }
    if ((4ULL & vlSelf->__VactTriggered.word(0U))) {
        vlSelf->__VdlySched.resume();
    }
}

void Vmac_tb___024root___timing_commit(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___timing_commit\n"); );
    // Body
    if ((! (2ULL & vlSelf->__VactTriggered.word(0U)))) {
        vlSelf->__VtrigSched_he644037f__0.commit("@(negedge tb_mac.clk)");
    }
}

void Vmac_tb___024root___eval_triggers__act(Vmac_tb___024root* vlSelf);

bool Vmac_tb___024root___eval_phase__act(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_phase__act\n"); );
    // Init
    VlTriggerVec<3> __VpreTriggered;
    CData/*0:0*/ __VactExecute;
    // Body
    Vmac_tb___024root___eval_triggers__act(vlSelf);
    Vmac_tb___024root___timing_commit(vlSelf);
    __VactExecute = vlSelf->__VactTriggered.any();
    if (__VactExecute) {
        __VpreTriggered.andNot(vlSelf->__VactTriggered, vlSelf->__VnbaTriggered);
        vlSelf->__VnbaTriggered.thisOr(vlSelf->__VactTriggered);
        Vmac_tb___024root___timing_resume(vlSelf);
        Vmac_tb___024root___eval_act(vlSelf);
    }
    return (__VactExecute);
}

bool Vmac_tb___024root___eval_phase__nba(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_phase__nba\n"); );
    // Init
    CData/*0:0*/ __VnbaExecute;
    // Body
    __VnbaExecute = vlSelf->__VnbaTriggered.any();
    if (__VnbaExecute) {
        Vmac_tb___024root___eval_nba(vlSelf);
        vlSelf->__VnbaTriggered.clear();
    }
    return (__VnbaExecute);
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__nba(Vmac_tb___024root* vlSelf);
#endif  // VL_DEBUG
#ifdef VL_DEBUG
VL_ATTR_COLD void Vmac_tb___024root___dump_triggers__act(Vmac_tb___024root* vlSelf);
#endif  // VL_DEBUG

void Vmac_tb___024root___eval(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval\n"); );
    // Init
    IData/*31:0*/ __VnbaIterCount;
    CData/*0:0*/ __VnbaContinue;
    // Body
    __VnbaIterCount = 0U;
    __VnbaContinue = 1U;
    while (__VnbaContinue) {
        if (VL_UNLIKELY((0x64U < __VnbaIterCount))) {
#ifdef VL_DEBUG
            Vmac_tb___024root___dump_triggers__nba(vlSelf);
#endif
            VL_FATAL_MT("mac_tb.v", 3, "", "NBA region did not converge.");
        }
        __VnbaIterCount = ((IData)(1U) + __VnbaIterCount);
        __VnbaContinue = 0U;
        vlSelf->__VactIterCount = 0U;
        vlSelf->__VactContinue = 1U;
        while (vlSelf->__VactContinue) {
            if (VL_UNLIKELY((0x64U < vlSelf->__VactIterCount))) {
#ifdef VL_DEBUG
                Vmac_tb___024root___dump_triggers__act(vlSelf);
#endif
                VL_FATAL_MT("mac_tb.v", 3, "", "Active region did not converge.");
            }
            vlSelf->__VactIterCount = ((IData)(1U) 
                                       + vlSelf->__VactIterCount);
            vlSelf->__VactContinue = 0U;
            if (Vmac_tb___024root___eval_phase__act(vlSelf)) {
                vlSelf->__VactContinue = 1U;
            }
        }
        if (Vmac_tb___024root___eval_phase__nba(vlSelf)) {
            __VnbaContinue = 1U;
        }
    }
}

#ifdef VL_DEBUG
void Vmac_tb___024root___eval_debug_assertions(Vmac_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vmac_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vmac_tb___024root___eval_debug_assertions\n"); );
}
#endif  // VL_DEBUG
