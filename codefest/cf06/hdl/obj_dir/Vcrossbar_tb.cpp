// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Model implementation (design independent parts)

#include "Vcrossbar_tb__pch.h"
#include "verilated_vcd_c.h"

//============================================================
// Constructors

Vcrossbar_tb::Vcrossbar_tb(VerilatedContext* _vcontextp__, const char* _vcname__)
    : VerilatedModel{*_vcontextp__}
    , vlSymsp{new Vcrossbar_tb__Syms(contextp(), _vcname__, this)}
    , rootp{&(vlSymsp->TOP)}
{
    // Register model with the context
    contextp()->addModel(this);
}

Vcrossbar_tb::Vcrossbar_tb(const char* _vcname__)
    : Vcrossbar_tb(Verilated::threadContextp(), _vcname__)
{
}

//============================================================
// Destructor

Vcrossbar_tb::~Vcrossbar_tb() {
    delete vlSymsp;
}

//============================================================
// Evaluation function

#ifdef VL_DEBUG
void Vcrossbar_tb___024root___eval_debug_assertions(Vcrossbar_tb___024root* vlSelf);
#endif  // VL_DEBUG
void Vcrossbar_tb___024root___eval_static(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval_initial(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval_settle(Vcrossbar_tb___024root* vlSelf);
void Vcrossbar_tb___024root___eval(Vcrossbar_tb___024root* vlSelf);

void Vcrossbar_tb::eval_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+++++TOP Evaluate Vcrossbar_tb::eval_step\n"); );
#ifdef VL_DEBUG
    // Debug assertions
    Vcrossbar_tb___024root___eval_debug_assertions(&(vlSymsp->TOP));
#endif  // VL_DEBUG
    vlSymsp->__Vm_activity = true;
    vlSymsp->__Vm_deleter.deleteAll();
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) {
        vlSymsp->__Vm_didInit = true;
        VL_DEBUG_IF(VL_DBG_MSGF("+ Initial\n"););
        Vcrossbar_tb___024root___eval_static(&(vlSymsp->TOP));
        Vcrossbar_tb___024root___eval_initial(&(vlSymsp->TOP));
        Vcrossbar_tb___024root___eval_settle(&(vlSymsp->TOP));
    }
    VL_DEBUG_IF(VL_DBG_MSGF("+ Eval\n"););
    Vcrossbar_tb___024root___eval(&(vlSymsp->TOP));
    // Evaluate cleanup
    Verilated::endOfEval(vlSymsp->__Vm_evalMsgQp);
}

void Vcrossbar_tb::eval_end_step() {
    VL_DEBUG_IF(VL_DBG_MSGF("+eval_end_step Vcrossbar_tb::eval_end_step\n"); );
#ifdef VM_TRACE
    // Tracing
    if (VL_UNLIKELY(vlSymsp->__Vm_dumping)) vlSymsp->_traceDump();
#endif  // VM_TRACE
}

//============================================================
// Events and timing
bool Vcrossbar_tb::eventsPending() { return !vlSymsp->TOP.__VdlySched.empty(); }

uint64_t Vcrossbar_tb::nextTimeSlot() { return vlSymsp->TOP.__VdlySched.nextTimeSlot(); }

//============================================================
// Utilities

const char* Vcrossbar_tb::name() const {
    return vlSymsp->name();
}

//============================================================
// Invoke final blocks

void Vcrossbar_tb___024root___eval_final(Vcrossbar_tb___024root* vlSelf);

VL_ATTR_COLD void Vcrossbar_tb::final() {
    Vcrossbar_tb___024root___eval_final(&(vlSymsp->TOP));
}

//============================================================
// Implementations of abstract methods from VerilatedModel

const char* Vcrossbar_tb::hierName() const { return vlSymsp->name(); }
const char* Vcrossbar_tb::modelName() const { return "Vcrossbar_tb"; }
unsigned Vcrossbar_tb::threads() const { return 1; }
void Vcrossbar_tb::prepareClone() const { contextp()->prepareClone(); }
void Vcrossbar_tb::atClone() const {
    contextp()->threadPoolpOnClone();
}
std::unique_ptr<VerilatedTraceConfig> Vcrossbar_tb::traceConfig() const {
    return std::unique_ptr<VerilatedTraceConfig>{new VerilatedTraceConfig{false, false, false}};
};

//============================================================
// Trace configuration

void Vcrossbar_tb___024root__trace_decl_types(VerilatedVcd* tracep);

void Vcrossbar_tb___024root__trace_init_top(Vcrossbar_tb___024root* vlSelf, VerilatedVcd* tracep);

VL_ATTR_COLD static void trace_init(void* voidSelf, VerilatedVcd* tracep, uint32_t code) {
    // Callback from tracep->open()
    Vcrossbar_tb___024root* const __restrict vlSelf VL_ATTR_UNUSED = static_cast<Vcrossbar_tb___024root*>(voidSelf);
    Vcrossbar_tb__Syms* const __restrict vlSymsp VL_ATTR_UNUSED = vlSelf->vlSymsp;
    if (!vlSymsp->_vm_contextp__->calcUnusedSigs()) {
        VL_FATAL_MT(__FILE__, __LINE__, __FILE__,
            "Turning on wave traces requires Verilated::traceEverOn(true) call before time 0.");
    }
    vlSymsp->__Vm_baseCode = code;
    tracep->pushPrefix(std::string{vlSymsp->name()}, VerilatedTracePrefixType::SCOPE_MODULE);
    Vcrossbar_tb___024root__trace_decl_types(tracep);
    Vcrossbar_tb___024root__trace_init_top(vlSelf, tracep);
    tracep->popPrefix();
}

VL_ATTR_COLD void Vcrossbar_tb___024root__trace_register(Vcrossbar_tb___024root* vlSelf, VerilatedVcd* tracep);

VL_ATTR_COLD void Vcrossbar_tb::trace(VerilatedVcdC* tfp, int levels, int options) {
    if (tfp->isOpen()) {
        vl_fatal(__FILE__, __LINE__, __FILE__,"'Vcrossbar_tb::trace()' shall not be called after 'VerilatedVcdC::open()'.");
    }
    if (false && levels && options) {}  // Prevent unused
    tfp->spTrace()->addModel(this);
    tfp->spTrace()->addInitCb(&trace_init, &(vlSymsp->TOP));
    Vcrossbar_tb___024root__trace_register(&(vlSymsp->TOP), tfp->spTrace());
}
