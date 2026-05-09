// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Tracing implementation internals
#include "verilated_vcd_c.h"
#include "Vcrossbar_tb__Syms.h"


void Vcrossbar_tb___024root__trace_chg_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp);

void Vcrossbar_tb___024root__trace_chg_0(void* voidSelf, VerilatedVcd::Buffer* bufp) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_chg_0\n"); );
    // Init
    Vcrossbar_tb___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vcrossbar_tb___024root*>(voidSelf);
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    if (VL_UNLIKELY(!vlSymsp->__Vm_activity)) return;
    // Body
    Vcrossbar_tb___024root__trace_chg_0_sub_0((&vlSymsp->TOP), bufp);
}

void Vcrossbar_tb___024root__trace_chg_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_chg_0_sub_0\n"); );
    // Init
    uint32_t* const oldp VL_ATTR_UNUSED = bufp->oldp(vlSymsp->__Vm_baseCode + 1);
    // Body
    if (VL_UNLIKELY((vlSelf->__Vm_traceActivity[1U] 
                     | vlSelf->__Vm_traceActivity[2U]))) {
        bufp->chgCData(oldp+0,(vlSelf->crossbar_tb__DOT__acts_i[0]),8);
        bufp->chgCData(oldp+1,(vlSelf->crossbar_tb__DOT__acts_i[1]),8);
        bufp->chgCData(oldp+2,(vlSelf->crossbar_tb__DOT__acts_i[2]),8);
        bufp->chgCData(oldp+3,(vlSelf->crossbar_tb__DOT__acts_i[3]),8);
        bufp->chgSData(oldp+4,(vlSelf->crossbar_tb__DOT__weights_i),16);
    }
    if (VL_UNLIKELY((vlSelf->__Vm_traceActivity[3U] 
                     | vlSelf->__Vm_traceActivity[4U]))) {
        bufp->chgSData(oldp+5,(vlSelf->crossbar_tb__DOT__mac_outs_o[0]),10);
        bufp->chgSData(oldp+6,(vlSelf->crossbar_tb__DOT__mac_outs_o[1]),10);
        bufp->chgSData(oldp+7,(vlSelf->crossbar_tb__DOT__mac_outs_o[2]),10);
        bufp->chgSData(oldp+8,(vlSelf->crossbar_tb__DOT__mac_outs_o[3]),10);
        bufp->chgSData(oldp+9,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                               [0U][0U]),9);
        bufp->chgSData(oldp+10,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [0U][1U]),9);
        bufp->chgSData(oldp+11,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [0U][2U]),9);
        bufp->chgSData(oldp+12,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [0U][3U]),9);
        bufp->chgSData(oldp+13,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [1U][0U]),9);
        bufp->chgSData(oldp+14,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [1U][1U]),9);
        bufp->chgSData(oldp+15,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [1U][2U]),9);
        bufp->chgSData(oldp+16,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [1U][3U]),9);
        bufp->chgSData(oldp+17,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [2U][0U]),9);
        bufp->chgSData(oldp+18,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [2U][1U]),9);
        bufp->chgSData(oldp+19,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [2U][2U]),9);
        bufp->chgSData(oldp+20,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [2U][3U]),9);
        bufp->chgSData(oldp+21,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [3U][0U]),9);
        bufp->chgSData(oldp+22,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [3U][1U]),9);
        bufp->chgSData(oldp+23,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [3U][2U]),9);
        bufp->chgSData(oldp+24,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                                [3U][3U]),9);
    }
}

void Vcrossbar_tb___024root__trace_cleanup(void* voidSelf, VerilatedVcd* /*unused*/) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_cleanup\n"); );
    // Init
    Vcrossbar_tb___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vcrossbar_tb___024root*>(voidSelf);
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    // Body
    vlSymsp->__Vm_activity = false;
    vlSymsp->TOP.__Vm_traceActivity[0U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[1U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[2U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[3U] = 0U;
    vlSymsp->TOP.__Vm_traceActivity[4U] = 0U;
}
