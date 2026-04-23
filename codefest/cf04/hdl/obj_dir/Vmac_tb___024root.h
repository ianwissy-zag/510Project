// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vmac_tb.h for the primary calling header

#ifndef VERILATED_VMAC_TB___024ROOT_H_
#define VERILATED_VMAC_TB___024ROOT_H_  // guard

#include "verilated.h"
#include "verilated_timing.h"


class Vmac_tb__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vmac_tb___024root final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    CData/*0:0*/ tb_mac__DOT__clk;
    CData/*0:0*/ tb_mac__DOT__rst;
    CData/*7:0*/ tb_mac__DOT__a;
    CData/*7:0*/ tb_mac__DOT__b;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __Vtrigprevexpr___TOP__tb_mac__DOT__clk__0;
    CData/*0:0*/ __VactContinue;
    SData/*15:0*/ tb_mac__DOT__uut__DOT__product;
    IData/*31:0*/ tb_mac__DOT__out;
    IData/*31:0*/ tb_mac__DOT__error_count;
    IData/*31:0*/ __VactIterCount;
    VlDelayScheduler __VdlySched;
    VlTriggerScheduler __VtrigSched_he644037f__0;
    VlTriggerVec<1> __VstlTriggered;
    VlTriggerVec<3> __VactTriggered;
    VlTriggerVec<3> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vmac_tb__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vmac_tb___024root(Vmac_tb__Syms* symsp, const char* v__name);
    ~Vmac_tb___024root();
    VL_UNCOPYABLE(Vmac_tb___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
