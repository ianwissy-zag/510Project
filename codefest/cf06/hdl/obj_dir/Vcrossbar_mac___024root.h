// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vcrossbar_mac.h for the primary calling header

#ifndef VERILATED_VCROSSBAR_MAC___024ROOT_H_
#define VERILATED_VCROSSBAR_MAC___024ROOT_H_  // guard

#include "verilated.h"
#include "verilated_timing.h"


class Vcrossbar_mac__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vcrossbar_mac___024root final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    SData/*15:0*/ tb_mac_crossbar_4x4__DOT__weights_i;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __VactContinue;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<CData/*7:0*/, 4> tb_mac_crossbar_4x4__DOT__acts_i;
    VlUnpacked<SData/*9:0*/, 4> tb_mac_crossbar_4x4__DOT__mac_outs_o;
    VlUnpacked<VlUnpacked<SData/*8:0*/, 4>, 4> tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results;
    VlDelayScheduler __VdlySched;
    VlTriggerVec<1> __VstlTriggered;
    VlTriggerVec<1> __VactTriggered;
    VlTriggerVec<1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vcrossbar_mac__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vcrossbar_mac___024root(Vcrossbar_mac__Syms* symsp, const char* v__name);
    ~Vcrossbar_mac___024root();
    VL_UNCOPYABLE(Vcrossbar_mac___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
