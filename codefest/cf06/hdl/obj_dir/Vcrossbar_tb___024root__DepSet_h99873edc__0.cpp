// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vcrossbar_tb.h for the primary calling header

#include "Vcrossbar_tb__pch.h"
#include "Vcrossbar_tb__Syms.h"
#include "Vcrossbar_tb___024root.h"

VL_INLINE_OPT VlCoroutine Vcrossbar_tb___024root___eval_initial__TOP__Vtiming__0(Vcrossbar_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root___eval_initial__TOP__Vtiming__0\n"); );
    // Init
    VlWide<3>/*95:0*/ __Vtemp_1;
    // Body
    __Vtemp_1[0U] = 0x2e766364U;
    __Vtemp_1[1U] = 0x635f7462U;
    __Vtemp_1[2U] = 0x6d61U;
    vlSymsp->_vm_contextp__->dumpfile(VL_CVT_PACK_STR_NW(3, __Vtemp_1));
    VL_PRINTF_MT("-Info: crossbar_tb.sv:38: $dumpvar ignored, as Verilated without --trace\n");
    VL_WRITEF("===============================================================\n   4x4 MAC Crossbar Testbench (clocked, synchronous reset)\n   Weight Encoding: 0 = +1, 1 = -1\n===============================================================\n\n");
    vlSelf->crossbar_tb__DOT__rst = 1U;
    vlSelf->crossbar_tb__DOT__acts_i[3U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[2U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[1U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[0U] = 0U;
    vlSelf->crossbar_tb__DOT__weights_i = 0U;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       49);
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       49);
    vlSelf->crossbar_tb__DOT__rst = 0U;
    VL_WRITEF("Test Case 1: Baseline\n");
    vlSelf->crossbar_tb__DOT__acts_i[0U] = 0xaU;
    vlSelf->crossbar_tb__DOT__acts_i[1U] = 0x14U;
    vlSelf->crossbar_tb__DOT__acts_i[2U] = 0x1eU;
    vlSelf->crossbar_tb__DOT__acts_i[3U] = 0x28U;
    vlSelf->crossbar_tb__DOT__weights_i = 0x79caU;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       73);
    co_await vlSelf->__VdlySched.delay(0x3e8ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       74);
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: -40, Col1: 0, Col2: -20, Col3: -20\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\nTest Case 2: Negative Inputs / Extreme Values\n",
              8,vlSelf->crossbar_tb__DOT__acts_i[0U],
              8,vlSelf->crossbar_tb__DOT__acts_i[1U],
              8,vlSelf->crossbar_tb__DOT__acts_i[2U],
              8,vlSelf->crossbar_tb__DOT__acts_i[3U],
              10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [0U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [1U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [2U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [3U]);
    vlSelf->crossbar_tb__DOT__acts_i[0U] = 0x80U;
    vlSelf->crossbar_tb__DOT__acts_i[1U] = 0x7fU;
    vlSelf->crossbar_tb__DOT__acts_i[2U] = 0xceU;
    vlSelf->crossbar_tb__DOT__acts_i[3U] = 0U;
    vlSelf->crossbar_tb__DOT__weights_i = 0x6a6aU;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       105);
    co_await vlSelf->__VdlySched.delay(0x3e8ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       106);
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: -51, Col1: 51, Col2: -305, Col3: 305\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\nTest Case 3: All Zeros\n",
              8,vlSelf->crossbar_tb__DOT__acts_i[0U],
              8,vlSelf->crossbar_tb__DOT__acts_i[1U],
              8,vlSelf->crossbar_tb__DOT__acts_i[2U],
              8,vlSelf->crossbar_tb__DOT__acts_i[3U],
              10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [0U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [1U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [2U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [3U]);
    vlSelf->crossbar_tb__DOT__acts_i[0U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[1U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[2U] = 0U;
    vlSelf->crossbar_tb__DOT__acts_i[3U] = 0U;
    vlSelf->crossbar_tb__DOT__weights_i = 0xa5f0U;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       128);
    co_await vlSelf->__VdlySched.delay(0x3e8ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       129);
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: 0, Col1: 0, Col2: 0, Col3: 0\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\nTest Case 4: Synchronous reset clears outputs\n",
              8,vlSelf->crossbar_tb__DOT__acts_i[0U],
              8,vlSelf->crossbar_tb__DOT__acts_i[1U],
              8,vlSelf->crossbar_tb__DOT__acts_i[2U],
              8,vlSelf->crossbar_tb__DOT__acts_i[3U],
              10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [0U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [1U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [2U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [3U]);
    vlSelf->crossbar_tb__DOT__acts_i[0U] = 5U;
    vlSelf->crossbar_tb__DOT__acts_i[1U] = 5U;
    vlSelf->crossbar_tb__DOT__acts_i[2U] = 5U;
    vlSelf->crossbar_tb__DOT__acts_i[3U] = 5U;
    vlSelf->crossbar_tb__DOT__weights_i = 0U;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       145);
    co_await vlSelf->__VdlySched.delay(0x3e8ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       146);
    VL_WRITEF("Before reset \342\200\224 Actual Outputs: Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n",
              10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [0U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [1U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [2U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [3U]);
    vlSelf->crossbar_tb__DOT__rst = 1U;
    co_await vlSelf->__VtrigSched_h99f9162d__0.trigger(0U, 
                                                       nullptr, 
                                                       "@(posedge crossbar_tb.clk)", 
                                                       "crossbar_tb.sv", 
                                                       151);
    co_await vlSelf->__VdlySched.delay(0x3e8ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       152);
    VL_WRITEF("After  reset \342\200\224 Expected: 0,0,0,0  Actual: Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\n",
              10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [0U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [1U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [2U],10,vlSelf->crossbar_tb__DOT__mac_outs_o
              [3U]);
    vlSelf->crossbar_tb__DOT__rst = 0U;
    VL_WRITEF("===============================================================\n   Simulation Complete\n===============================================================\n");
    VL_FINISH_MT("crossbar_tb.sv", 161, "");
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vcrossbar_tb___024root___dump_triggers__act(Vcrossbar_tb___024root* vlSelf);
#endif  // VL_DEBUG

void Vcrossbar_tb___024root___eval_triggers__act(Vcrossbar_tb___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root___eval_triggers__act\n"); );
    // Body
    vlSelf->__VactTriggered.set(0U, ((IData)(vlSelf->crossbar_tb__DOT__clk) 
                                     & (~ (IData)(vlSelf->__Vtrigprevexpr___TOP__crossbar_tb__DOT__clk__0))));
    vlSelf->__VactTriggered.set(1U, vlSelf->__VdlySched.awaitingCurrentTime());
    vlSelf->__Vtrigprevexpr___TOP__crossbar_tb__DOT__clk__0 
        = vlSelf->crossbar_tb__DOT__clk;
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vcrossbar_tb___024root___dump_triggers__act(vlSelf);
    }
#endif
}
