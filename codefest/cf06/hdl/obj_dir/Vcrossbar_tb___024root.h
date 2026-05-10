// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vcrossbar_tb.h for the primary calling header

#ifndef VERILATED_VCROSSBAR_TB___024ROOT_H_
#define VERILATED_VCROSSBAR_TB___024ROOT_H_  // guard

#include "verilated.h"
#include "verilated_timing.h"


class Vcrossbar_tb__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vcrossbar_tb___024root final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    CData/*0:0*/ crossbar_tb__DOT__clk;
    CData/*0:0*/ crossbar_tb__DOT__rst;
    SData/*15:0*/ crossbar_tb__DOT__weights_i;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __Vtrigprevexpr___TOP__crossbar_tb__DOT__clk__0;
    CData/*0:0*/ __VactContinue;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<CData/*7:0*/, 4> crossbar_tb__DOT__acts_i;
    VlUnpacked<CData/*7:0*/, 4> crossbar_tb__DOT__mac_outs_o;
    VlUnpacked<VlUnpacked<SData/*8:0*/, 4>, 4> crossbar_tb__DOT__dut__DOT__mult_results;
    VlUnpacked<SData/*10:0*/, 4> crossbar_tb__DOT__dut__DOT__col_sum;
    VlDelayScheduler __VdlySched;
    VlTriggerScheduler __VtrigSched_h99f9162d__0;
    VlTriggerVec<1> __VstlTriggered;
    VlTriggerVec<2> __VactTriggered;
    VlTriggerVec<2> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vcrossbar_tb__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vcrossbar_tb___024root(Vcrossbar_tb__Syms* symsp, const char* v__name);
    ~Vcrossbar_tb___024root();
    VL_UNCOPYABLE(Vcrossbar_tb___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
