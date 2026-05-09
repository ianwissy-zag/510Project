// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vtb_mac_crossbar_4x4.h for the primary calling header

#ifndef VERILATED_VTB_MAC_CROSSBAR_4X4___024ROOT_H_
#define VERILATED_VTB_MAC_CROSSBAR_4X4___024ROOT_H_  // guard

#include "verilated.h"
#include "verilated_timing.h"


class Vtb_mac_crossbar_4x4__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vtb_mac_crossbar_4x4___024root final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    SData/*15:0*/ tb_mac_crossbar_4x4__DOT__weights_i;
    CData/*0:0*/ __VstlFirstIteration;
    CData/*0:0*/ __VactContinue;
    IData/*31:0*/ __VactIterCount;
    VlUnpacked<CData/*7:0*/, 4> tb_mac_crossbar_4x4__DOT__acts_i;
    VlUnpacked<SData/*9:0*/, 4> tb_mac_crossbar_4x4__DOT__mac_outs_o;
    VlUnpacked<VlUnpacked<SData/*8:0*/, 4>, 4> tb_mac_crossbar_4x4__DOT__dut__DOT__mult_results;
    VlUnpacked<CData/*0:0*/, 5> __Vm_traceActivity;
    VlDelayScheduler __VdlySched;
    VlTriggerVec<1> __VstlTriggered;
    VlTriggerVec<1> __VactTriggered;
    VlTriggerVec<1> __VnbaTriggered;

    // INTERNAL VARIABLES
    Vtb_mac_crossbar_4x4__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vtb_mac_crossbar_4x4___024root(Vtb_mac_crossbar_4x4__Syms* symsp, const char* v__name);
    ~Vtb_mac_crossbar_4x4___024root();
    VL_UNCOPYABLE(Vtb_mac_crossbar_4x4___024root);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
