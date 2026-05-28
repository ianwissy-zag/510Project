# M4 Milestone — 32×32 BF16 Systolic Array Accelerator

This directory contains all deliverables for the M4 milestone: RTL source, simulation results, synthesis results, benchmark data, and the written report.

---

## Directory Structure

### `rtl/` — SystemVerilog RTL Source

Snapshot of the 32×32 weight-stationary systolic array as of M4. The active development copies live in `../HDL_Systolic_32x32/`; these files represent the frozen milestone state.

| File | Description |
|------|-------------|
| `top.sv` | Top-level module (`sys_top`): wires AXI, weight SRAMs, stagger, systolic array, controller, accumulator SRAM, and readback |
| `controller.sv` | FSM: IDLE → LOAD_WT (32 cycles) → STREAM (M_count+32+1 cycles) → DONE |
| `axi.sv` | AXI4-Stream slave: demuxes tuser-encoded beats to weight SRAMs, activation buffer, or bias SRAM |
| `systolic_32x32.sv` | 32×32 PE mesh; weight-stationary with FP32 psum propagation |
| `pe.sv` | Single processing element: one `bf16_mac_unit` + psum register |
| `bf16_mac_unit.sv` | BF16 multiply + FP32 accumulate (wraps `bf16_mul` and `bf16_fp32_add`) |
| `bf16_mul.sv` | Custom 8×8-bit mantissa multiplier for BF16 inputs |
| `bf16_fp32_add.sv` | FP32 adder for accumulation |
| `act_stagger.sv` | Triangular shift-register bank: delays activation row `r` by `r+1` cycles to align the wavefront |
| `accum_sram.sv` | Dual-port FP32 accumulator SRAM (M_MAX=256 rows × 32 columns) |
| `weight_sram.sv` | Ping-pong weight SRAM (2 banks × 32 rows × 32 BF16 columns) |
| `bias_sram.sv` | Bias SRAM (32 FP32 values, loaded via AXI tuser=110) |
| `axi_readback.sv` | AXI4-Stream master: streams accumulator rows out with optional bias addition and GELU post-processing |
| `gelu_unit.sv` | Padé [3/2] GELU approximation, 8-stage pipeline |
| `fp32_recip.sv` | FP32 reciprocal unit used inside the GELU pipeline |
| `output_sram.sv` | Output staging SRAM for readback buffering |

### `tb/` — Testbench

| File | Description |
|------|-------------|
| `tb_top.cpp` | Verilator C++ testbench: 16 directed test cases covering basic matmul, K-tile accumulation, bias, GELU, backward mode, M=1/M=256 boundaries, ping-pong weight preloading |

### `sim/` — Simulation Results

| File | Description |
|------|-------------|
| `final_run.log` | Verilator testbench output: all 16 tests pass (`All 32x32 tests PASSED (16/16)`) |
| `final_waveform.png` | GTKWave screenshot showing LOAD_WT → STREAM → DONE state transitions and AXI bus activity |

### `synth/` — Cadence Genus Synthesis Results

Target: ASAP7 predictive 7nm RVT library, 1650 ps clock period (606 MHz target).

| File | Description |
|------|-------------|
| `genus.tcl` | Synthesis script: sets library paths, reads RTL, applies constraints, runs `syn_generic`/`syn_map`/`syn_opt` |
| `constraints.sdc` | Timing constraints: 1650 ps clock, input/output delay margins |
| `genus.log` | Full Cadence Genus run log |
| `area_report.txt` | Cell-level area breakdown; total 385,241 µm² (0.385 mm²); systolic array 171,343 µm², accumulator SRAM 144,918 µm² |
| `power_report.txt` | Vectorless power estimate: 0.900 W total (register 40.8%, logic 59.2%) |
| `timing_report.txt` | Detailed timing paths; worst violator is GELU pipeline stage 2→3 at −1 ps slack |
| `qor.rpt` | Quality-of-results summary: WNS = −0.9 ps, TNS = −85.3 ps, 154 violating paths (all in GELU units) |

### `bench/` — Benchmark Data

| File | Description |
|------|-------------|
| `benchmark.md` | Formatted benchmark results: CPU vs. Verilator-accelerated timing, energy measurements, roofline analysis, and speedup table |
| `benchmar_data.txt` | Raw key-value timing data from both software and `systolic_vrl` backends (used to generate `benchmark.md`) |
| `roofline_final.png` | Roofline plot showing accelerator operating point at 563 GFLOP/s / ~32.3 FLOP/B on the memory-bandwidth slope |
| `weight_accuracy_final.out` | Weight accuracy comparison (CPU vs. accelerated): 124M parameters, mean absolute difference 9.65×10⁻⁵ (0.0175% relative) |

### `report/` — Written Report

| File | Description |
|------|-------------|
| `design_justification.pdf` | M4 design justification report covering BF16 precision rationale, architecture decisions, AXI interface, synthesis results, and performance analysis |
| `figures/Block_Design.png` | Block diagram of the full `sys_top` datapath |
| `figures/roofline_final.png` | Roofline figure as included in the report |

---

## Key Results Summary

| Metric | Value |
|--------|-------|
| Technology | ASAP7 predictive 7nm RVT |
| Clock target | 606 MHz (1650 ps period) |
| PE array | 32×32 = 1,024 BF16 MACs |
| Peak throughput | 1,241 GFLOP/s |
| Effective throughput | 563 GFLOP/s (45.4% PE utilization) |
| Total cell area | 385,241 µm² (0.385 mm²) |
| Power (vectorless) | 0.900 W |
| WNS | −0.9 ps (154 violations, all in GELU pipeline) |
| Simulation | 16/16 directed tests pass |
| Speedup vs. CPU matmul | ~13.5× (Verilator-projected wall time) |
| Energy reduction | ~432× vs. CPU (Intel RAPL measured) |
