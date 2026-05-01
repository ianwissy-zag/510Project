# M2 Simulation Reproduction Guide

## Directory Structure

```
m2/
├── rtl/                  — RTL source files
│   ├── compute_core.sv   — 128-wide BF16 vector MAC array (core compute unit)
│   ├── interface.sv      — AXI4-Stream input buffer (weight + activation routing)
│   ├── top.sv            — Top-level wrapper (vec_mac_top)
│   ├── bf16_mac_unit.sv  — Per-lane BF16 MAC unit (simulation + synthesis paths)
│   ├── bf16_mul.sv       — Custom BF16×BF16 multiplier
│   ├── bf16_fp32_add.sv  — FP32 adder for accumulation
│   ├── bf16_mac_unit_core.v — Berkeley HardFloat FP32 MAC (synthesis reference)
│   ├── weight_sram.sv    — Weight SRAM (ping/pong banks)
│   ├── act_sram.sv       — Activation SRAM (ping/pong banks)
│   ├── output_sram.sv    — Output SRAM (FP32 partial sums)
│   ├── controller.sv     — Dataflow controller state machine
│   ├── axi_readback.sv   — AXI-Stream readback of FP32 results
│   └── Makefile
├── tb/                   — Testbench source files
│   ├── tb_compute_core.cpp  — C++ testbench for the full compute core (Verilator)
│   ├── tb_interface.sv      — Verilog testbench for the AXI interface module
│   ├── tb_bf16_mul.cpp      — C++ testbench for bf16_mul
│   ├── tb_bf16_fp32_add.cpp — C++ testbench for bf16_fp32_add
│   └── bf16_mac_dpi.c       — DPI-C helper for BF16 arithmetic in simulation
├── sim/                  — Simulation run outputs and compiled binaries
│   ├── compute_core_run.log — Output of compute core testbench
│   ├── interface_run.log    — Output of AXI interface testbench
│   ├── bf16_mul_run.log     — Output of BF16 multiplier testbench
│   └── bf16_fp32_add_run.log — Output of FP32 adder testbench
├── precision.md          — Precision format selection rationale
└── README.md             — This file
```

## Prerequisites

| Tool | Purpose | Install |
|------|---------|---------|
| Verilator ≥ 5.0 | SystemVerilog simulation (C++ testbenches) | `sudo apt install verilator` |
| Icarus Verilog | Verilog simulation (tb_interface.sv) | `sudo apt install iverilog` |
| g++ | C++ compilation | `sudo apt install g++` |

## Running the Simulations

All commands are run from the `m2/rtl/` directory.

### Run all testbenches at once

```bash
cd m2/rtl
make run_all
```

This runs all four testbenches and writes logs to `m2/tb/`.

### Run individual testbenches

```bash
# Full compute core (vec_mac_top) — output → tb/compute_core_run.log
make run

# AXI interface module — output → tb/interface_run.log
make run_interface

# BF16 multiplier unit — output → tb/bf16_mul_run.log
make run_mul

# FP32 adder unit — output → tb/bf16_fp32_add_run.log
make run_add
```

### Build without running

```bash
make all
```

### Clean build artifacts

```bash
make clean
```

## Expected Output

All testbenches should report PASSED with no failures:

```
All BF16 vec_mac_top tests PASSED.
All axi_interface tests PASSED.
All bf16_mul tests PASSED.
All bf16_fp32_add tests PASSED.
```

## Test Coverage

### Compute core (`tb_compute_core.cpp`)

End-to-end test of the full accelerator via AXI-Stream protocol:

- **Test 1** — All BF16(1.0) weights and activations → FP32(32.0) per lane
- **Test 2** — BF16(1.0) weights, BF16(1..32) activations → FP32(528.0) per lane
- **Test 3** — Two-tile accumulation with ping-pong weight banks → FP32(64.0) per lane

### AXI interface (`tb_interface.sv`)

Unit test of AXI-Stream routing and SRAM write-enable logic:

- **Test 1** — Weight beats routed to ping bank (`tuser=00`), `wt_we_0` fires on last beat
- **Test 2** — Weight beats routed to pong bank (`tuser=01`), `wt_we_1` fires on last beat
- **Test 3** — Activation beat routed to act ping (`tuser=10`), `act_we_0` fires
- **Test 4** — Activation beat routed to act pong (`tuser=11`), `act_we_1` fires
- **Test 5** — Routing locked to `tuser` from beat 0; mid-packet changes ignored
- **Test 6** — No write enables asserted when `tvalid=0`

### BF16 multiplier (`tb_bf16_mul.cpp`)

Unit test of the custom 8×8 mantissa BF16 multiplier:

- Basic positive values, sign combinations, zero inputs
- 1000 random normal pairs verified against C float reference
- Overflow to infinity, small values, boundary cases

### FP32 adder (`tb_bf16_fp32_add.cpp`)

Unit test of the FP32 accumulation adder:

- Basic addition, subtraction paths, zero and infinity handling
- Accumulation chains matching expected MAC testbench values
- 1000 random FP32 pairs verified against C float reference (±2 ULP tolerance)
