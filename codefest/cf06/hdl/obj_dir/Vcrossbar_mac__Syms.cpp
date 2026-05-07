// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Symbol table implementation internals

#include "Vcrossbar_mac__pch.h"
#include "Vcrossbar_mac.h"
#include "Vcrossbar_mac___024root.h"

// FUNCTIONS
Vcrossbar_mac__Syms::~Vcrossbar_mac__Syms()
{
}

Vcrossbar_mac__Syms::Vcrossbar_mac__Syms(VerilatedContext* contextp, const char* namep, Vcrossbar_mac* modelp)
    : VerilatedSyms{contextp}
    // Setup internal state of the Syms class
    , __Vm_modelp{modelp}
    // Setup module instances
    , TOP{this, namep}
{
    // Configure time unit / time precision
    _vm_contextp__->timeunit(-9);
    _vm_contextp__->timeprecision(-12);
    // Setup each module's pointers to their submodules
    // Setup each module's pointer back to symbol table (for public functions)
    TOP.__Vconfigure(true);
}
