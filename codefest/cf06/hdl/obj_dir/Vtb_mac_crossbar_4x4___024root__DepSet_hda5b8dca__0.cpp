// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vtb_mac_crossbar_4x4.h for the primary calling header

#include "Vtb_mac_crossbar_4x4__pch.h"
#include "Vtb_mac_crossbar_4x4__Syms.h"
#include "Vtb_mac_crossbar_4x4___024root.h"

VL_INLINE_OPT VlCoroutine Vtb_mac_crossbar_4x4___024root___eval_initial__TOP__Vtiming__0(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_initial__TOP__Vtiming__0\n"); );
    // Init
    VlWide<3>/*95:0*/ __Vtemp_1;
    // Body
    __Vtemp_1[0U] = 0x2e766364U;
    __Vtemp_1[1U] = 0x635f7462U;
    __Vtemp_1[2U] = 0x6d61U;
    vlSymsp->_vm_contextp__->dumpfile(VL_CVT_PACK_STR_NW(3, __Vtemp_1));
    vlSymsp->_traceDumpOpen();
    VL_WRITEF("===============================================================\n   4x4 MAC Crossbar Testbench\n   Weight Encoding: 0 = +1, 1 = -1\n===============================================================\n\nTest Case 1: User Request\n");
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[0U] = 0xaU;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[1U] = 0x14U;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[2U] = 0x1eU;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[3U] = 0x28U;
    vlSelf->tb_mac_crossbar_4x4__DOT__weights_i = 0x79caU;
    co_await vlSelf->__VdlySched.delay(0x2710ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       61);
    vlSelf->__Vm_traceActivity[2U] = 1U;
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: -40, Col1: 0, Col2: -20, Col3: -20\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\nTest Case 2: Negative Inputs / Extreme Values\n",
              8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [0U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [1U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [2U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [3U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [0U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [1U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [2U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [3U]);
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[0U] = 0x80U;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[1U] = 0x7fU;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[2U] = 0xceU;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[3U] = 0U;
    vlSelf->tb_mac_crossbar_4x4__DOT__weights_i = 0x6a6aU;
    co_await vlSelf->__VdlySched.delay(0x2710ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       89);
    vlSelf->__Vm_traceActivity[2U] = 1U;
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: -51, Col1: 51, Col2: -305, Col3: 305\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\nTest Case 3: All Zeros\n",
              8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [0U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [1U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [2U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [3U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [0U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [1U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [2U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [3U]);
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[0U] = 0U;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[1U] = 0U;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[2U] = 0U;
    vlSelf->tb_mac_crossbar_4x4__DOT__acts_i[3U] = 0U;
    vlSelf->tb_mac_crossbar_4x4__DOT__weights_i = 0xa5f0U;
    co_await vlSelf->__VdlySched.delay(0x2710ULL, nullptr, 
                                       "crossbar_tb.sv", 
                                       112);
    vlSelf->__Vm_traceActivity[2U] = 1U;
    VL_WRITEF("Inputs: [%0d, %0d, %0d, %0d]\nExpected Outputs: Col0: 0, Col1: 0, Col2: 0, Col3: 0\nActual Outputs  : Col0: %0d, Col1: %0d, Col2: %0d, Col3: %0d\n\n===============================================================\n   Simulation Complete\n===============================================================\n",
              8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [0U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [1U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [2U],8,vlSelf->tb_mac_crossbar_4x4__DOT__acts_i
              [3U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [0U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [1U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [2U],10,vlSelf->tb_mac_crossbar_4x4__DOT__mac_outs_o
              [3U]);
    VL_FINISH_MT("crossbar_tb.sv", 123, "");
    vlSelf->__Vm_traceActivity[2U] = 1U;
}

#ifdef VL_DEBUG
VL_ATTR_COLD void Vtb_mac_crossbar_4x4___024root___dump_triggers__act(Vtb_mac_crossbar_4x4___024root* vlSelf);
#endif  // VL_DEBUG

void Vtb_mac_crossbar_4x4___024root___eval_triggers__act(Vtb_mac_crossbar_4x4___024root* vlSelf) {
    if (false && vlSelf) {}  // Prevent unused
    Vtb_mac_crossbar_4x4__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vtb_mac_crossbar_4x4___024root___eval_triggers__act\n"); );
    // Body
    vlSelf->__VactTriggered.set(0U, vlSelf->__VdlySched.awaitingCurrentTime());
#ifdef VL_DEBUG
    if (VL_UNLIKELY(vlSymsp->_vm_contextp__->debug())) {
        Vtb_mac_crossbar_4x4___024root___dump_triggers__act(vlSelf);
    }
#endif
}
