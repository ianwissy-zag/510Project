// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Tracing implementation internals
#include "verilated_vcd_c.h"
#include "Vcrossbar_tb__Syms.h"


VL_ATTR_COLD void Vcrossbar_tb___024root__trace_init_sub__TOP__0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd* tracep) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_init_sub__TOP__0\n"); );
    // Init
    const int c = vlSymsp->__Vm_baseCode;
    // Body
    tracep->pushPrefix("crossbar_tb", VerilatedTracePrefixType::SCOPE_MODULE);
    tracep->declBus(c+26,0,"INPUT_WIDTH",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::PARAMETER, VerilatedTraceSigType::INT, false,-1, 31,0);
    tracep->declBus(c+27,0,"OUTPUT_WIDTH",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::PARAMETER, VerilatedTraceSigType::INT, false,-1, 31,0);
    tracep->pushPrefix("acts_i", VerilatedTracePrefixType::ARRAY_UNPACKED);
    for (int i = 0; i < 4; ++i) {
        tracep->declBus(c+1+i*1,0,"",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, true,(i+0), 7,0);
    }
    tracep->popPrefix();
    tracep->declBus(c+5,0,"weights_i",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 15,0);
    tracep->pushPrefix("mac_outs_o", VerilatedTracePrefixType::ARRAY_UNPACKED);
    for (int i = 0; i < 4; ++i) {
        tracep->declBus(c+6+i*1,0,"",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, true,(i+0), 9,0);
    }
    tracep->popPrefix();
    tracep->pushPrefix("dut", VerilatedTracePrefixType::SCOPE_MODULE);
    tracep->declBus(c+26,0,"INPUT_WIDTH",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::PARAMETER, VerilatedTraceSigType::INT, false,-1, 31,0);
    tracep->declBus(c+27,0,"OUTPUT_WIDTH",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::PARAMETER, VerilatedTraceSigType::INT, false,-1, 31,0);
    tracep->pushPrefix("acts_i", VerilatedTracePrefixType::ARRAY_UNPACKED);
    for (int i = 0; i < 4; ++i) {
        tracep->declBus(c+1+i*1,0,"",-1, VerilatedTraceSigDirection::INPUT, VerilatedTraceSigKind::WIRE, VerilatedTraceSigType::LOGIC, true,(i+0), 7,0);
    }
    tracep->popPrefix();
    tracep->declBus(c+5,0,"weights_i",-1, VerilatedTraceSigDirection::INPUT, VerilatedTraceSigKind::WIRE, VerilatedTraceSigType::LOGIC, false,-1, 15,0);
    tracep->pushPrefix("mac_outs_o", VerilatedTracePrefixType::ARRAY_UNPACKED);
    for (int i = 0; i < 4; ++i) {
        tracep->declBus(c+6+i*1,0,"",-1, VerilatedTraceSigDirection::OUTPUT, VerilatedTraceSigKind::WIRE, VerilatedTraceSigType::LOGIC, true,(i+0), 9,0);
    }
    tracep->popPrefix();
    tracep->pushPrefix("mult_results", VerilatedTracePrefixType::ARRAY_UNPACKED);
    tracep->pushPrefix("[0]", VerilatedTracePrefixType::ARRAY_UNPACKED);
    tracep->declBus(c+10,0,"[0]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+11,0,"[1]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+12,0,"[2]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+13,0,"[3]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->popPrefix();
    tracep->pushPrefix("[1]", VerilatedTracePrefixType::ARRAY_UNPACKED);
    tracep->declBus(c+14,0,"[0]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+15,0,"[1]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+16,0,"[2]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+17,0,"[3]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->popPrefix();
    tracep->pushPrefix("[2]", VerilatedTracePrefixType::ARRAY_UNPACKED);
    tracep->declBus(c+18,0,"[0]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+19,0,"[1]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+20,0,"[2]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+21,0,"[3]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->popPrefix();
    tracep->pushPrefix("[3]", VerilatedTracePrefixType::ARRAY_UNPACKED);
    tracep->declBus(c+22,0,"[0]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+23,0,"[1]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+24,0,"[2]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->declBus(c+25,0,"[3]",-1, VerilatedTraceSigDirection::NONE, VerilatedTraceSigKind::VAR, VerilatedTraceSigType::LOGIC, false,-1, 8,0);
    tracep->popPrefix();
    tracep->popPrefix();
    tracep->popPrefix();
    tracep->popPrefix();
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_init_top(Vcrossbar_tb___024root* vlSelf, VerilatedVcd* tracep) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_init_top\n"); );
    // Body
    Vcrossbar_tb___024root__trace_init_sub__TOP__0(vlSelf, tracep);
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_const_0(void* voidSelf, VerilatedVcd::Buffer* bufp);
VL_ATTR_COLD void Vcrossbar_tb___024root__trace_full_0(void* voidSelf, VerilatedVcd::Buffer* bufp);
void Vcrossbar_tb___024root__trace_chg_0(void* voidSelf, VerilatedVcd::Buffer* bufp);
void Vcrossbar_tb___024root__trace_cleanup(void* voidSelf, VerilatedVcd* /*unused*/);

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_register(Vcrossbar_tb___024root* vlSelf, VerilatedVcd* tracep) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_register\n"); );
    // Body
    tracep->addConstCb(&Vcrossbar_tb___024root__trace_const_0, 0U, vlSelf);
    tracep->addFullCb(&Vcrossbar_tb___024root__trace_full_0, 0U, vlSelf);
    tracep->addChgCb(&Vcrossbar_tb___024root__trace_chg_0, 0U, vlSelf);
    tracep->addCleanupCb(&Vcrossbar_tb___024root__trace_cleanup, vlSelf);
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_const_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp);

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_const_0(void* voidSelf, VerilatedVcd::Buffer* bufp) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_const_0\n"); );
    // Init
    Vcrossbar_tb___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vcrossbar_tb___024root*>(voidSelf);
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    // Body
    Vcrossbar_tb___024root__trace_const_0_sub_0((&vlSymsp->TOP), bufp);
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_const_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_const_0_sub_0\n"); );
    // Init
    uint32_t* const oldp VL_ATTR_UNUSED = bufp->oldp(vlSymsp->__Vm_baseCode);
    // Body
    bufp->fullIData(oldp+26,(8U),32);
    bufp->fullIData(oldp+27,(0xaU),32);
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_full_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp);

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_full_0(void* voidSelf, VerilatedVcd::Buffer* bufp) {
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_full_0\n"); );
    // Init
    Vcrossbar_tb___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vcrossbar_tb___024root*>(voidSelf);
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    // Body
    Vcrossbar_tb___024root__trace_full_0_sub_0((&vlSymsp->TOP), bufp);
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_full_0_sub_0(Vcrossbar_tb___024root* vlSelf, VerilatedVcd::Buffer* bufp) {
    if (false && vlSelf) {}  // Prevent unused
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    VL_DEBUG_IF(VL_DBG_MSGF("+    Vcrossbar_tb___024root__trace_full_0_sub_0\n"); );
    // Init
    uint32_t* const oldp VL_ATTR_UNUSED = bufp->oldp(vlSymsp->__Vm_baseCode);
    // Body
    bufp->fullCData(oldp+1,(vlSelf->crossbar_tb__DOT__acts_i[0]),8);
    bufp->fullCData(oldp+2,(vlSelf->crossbar_tb__DOT__acts_i[1]),8);
    bufp->fullCData(oldp+3,(vlSelf->crossbar_tb__DOT__acts_i[2]),8);
    bufp->fullCData(oldp+4,(vlSelf->crossbar_tb__DOT__acts_i[3]),8);
    bufp->fullSData(oldp+5,(vlSelf->crossbar_tb__DOT__weights_i),16);
    bufp->fullSData(oldp+6,(vlSelf->crossbar_tb__DOT__mac_outs_o[0]),10);
    bufp->fullSData(oldp+7,(vlSelf->crossbar_tb__DOT__mac_outs_o[1]),10);
    bufp->fullSData(oldp+8,(vlSelf->crossbar_tb__DOT__mac_outs_o[2]),10);
    bufp->fullSData(oldp+9,(vlSelf->crossbar_tb__DOT__mac_outs_o[3]),10);
    bufp->fullSData(oldp+10,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [0U][0U]),9);
    bufp->fullSData(oldp+11,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [0U][1U]),9);
    bufp->fullSData(oldp+12,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [0U][2U]),9);
    bufp->fullSData(oldp+13,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [0U][3U]),9);
    bufp->fullSData(oldp+14,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [1U][0U]),9);
    bufp->fullSData(oldp+15,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [1U][1U]),9);
    bufp->fullSData(oldp+16,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [1U][2U]),9);
    bufp->fullSData(oldp+17,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [1U][3U]),9);
    bufp->fullSData(oldp+18,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [2U][0U]),9);
    bufp->fullSData(oldp+19,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [2U][1U]),9);
    bufp->fullSData(oldp+20,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [2U][2U]),9);
    bufp->fullSData(oldp+21,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [2U][3U]),9);
    bufp->fullSData(oldp+22,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [3U][0U]),9);
    bufp->fullSData(oldp+23,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [3U][1U]),9);
    bufp->fullSData(oldp+24,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [3U][2U]),9);
    bufp->fullSData(oldp+25,(vlSelf->crossbar_tb__DOT__dut__DOT__mult_results
                             [3U][3U]),9);
}
