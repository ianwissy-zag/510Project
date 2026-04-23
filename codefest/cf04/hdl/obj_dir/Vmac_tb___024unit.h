// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design internal header
// See Vmac_tb.h for the primary calling header

#ifndef VERILATED_VMAC_TB___024UNIT_H_
#define VERILATED_VMAC_TB___024UNIT_H_  // guard

#include "verilated.h"
#include "verilated_timing.h"


class Vmac_tb__Syms;

class alignas(VL_CACHE_LINE_BYTES) Vmac_tb___024unit final : public VerilatedModule {
  public:

    // DESIGN SPECIFIC STATE
    CData/*0:0*/ __VmonitorOff;

    // INTERNAL VARIABLES
    Vmac_tb__Syms* const vlSymsp;

    // CONSTRUCTORS
    Vmac_tb___024unit(Vmac_tb__Syms* symsp, const char* v__name);
    ~Vmac_tb___024unit();
    VL_UNCOPYABLE(Vmac_tb___024unit);

    // INTERNAL METHODS
    void __Vconfigure(bool first);
};


#endif  // guard
