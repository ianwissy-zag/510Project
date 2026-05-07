// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Symbol table internal header
//
// Internal details; most calling programs do not need this header,
// unless using verilator public meta comments.

#ifndef VERILATED_VCROSSBAR_MAC__SYMS_H_
#define VERILATED_VCROSSBAR_MAC__SYMS_H_  // guard

#include "verilated.h"

// INCLUDE MODEL CLASS

#include "Vcrossbar_mac.h"

// INCLUDE MODULE CLASSES
#include "Vcrossbar_mac___024root.h"

// SYMS CLASS (contains all model state)
class alignas(VL_CACHE_LINE_BYTES)Vcrossbar_mac__Syms final : public VerilatedSyms {
  public:
    // INTERNAL STATE
    Vcrossbar_mac* const __Vm_modelp;
    VlDeleter __Vm_deleter;
    bool __Vm_didInit = false;

    // MODULE INSTANCE STATE
    Vcrossbar_mac___024root        TOP;

    // CONSTRUCTORS
    Vcrossbar_mac__Syms(VerilatedContext* contextp, const char* namep, Vcrossbar_mac* modelp);
    ~Vcrossbar_mac__Syms();

    // METHODS
    const char* name() { return TOP.name(); }
};

#endif  // guard
