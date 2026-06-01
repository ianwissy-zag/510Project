# M4 Milestone — 32×32 BF16 Systolic Array Accelerator

This directory contains all deliverables for the M4 milestone: RTL source, simulation results, synthesis results, benchmark data, software HAL, and the written report.

---

## Directory Structure

### `rtl/` — SystemVerilog RTL Source

Snapshot of the 32×32 weight-stationary systolic array as of M4. The active development copies live in `../HDL_Systolic_32x32/`; these files represent the frozen milestone state.

| File | Description |
|------|-------------|
| `top.sv` | Top-level module (`sys_top`): wires AXI, shared ping-pong weight SRAMs, stagger, systolic array, controller, double-buffered accumulator SRAMs, and readback. Ports include `buf_sel`, `preload_rdy`, and `accum_buf_sel` |
| `controller.sv` | FSM: IDLE → LOAD_WT (32 cycles, skipped when `wt_preloaded_r=1`) → STREAM (M_count+34 cycles) → DONE. Includes drain-overlap LOAD_WT logic (`preload_rdy`, `ovlp_cnt`, `ovlp_active`, `wt_preloaded_r`) |
| `axi.sv` | AXI4-Stream slave: demuxes tuser-encoded beats to shared ping-pong weight SRAMs, activation buffer, or bias SRAM |
| `systolic_32x32.sv` | 32×32 PE mesh; weight-stationary with FP32 psum propagation |
| `pe.sv` | Single processing element: one `bf16_mac_unit` + psum register |
| `bf16_mac_unit.sv` | BF16 multiply + FP32 accumulate (wraps `bf16_mul` and `bf16_fp32_add`) |
| `bf16_mul.sv` | Custom 8×8-bit mantissa multiplier for BF16 inputs |
| `bf16_fp32_add.sv` | FP32 adder for accumulation |
| `fp32_mul.sv` | FP32 multiplier used in the GELU pipeline stages |
| `fp32_recip.sv` | FP32 reciprocal unit used inside the GELU pipeline |
| `act_stagger.sv` | Triangular shift-register bank: delays activation row `r` by `r+1` cycles to align the wavefront |
| `accum_sram.sv` | Dual-port FP32 accumulator SRAM (M_MAX=256 rows × 32 columns); instantiated twice for double-buffered ping-pong |
| `weight_sram.sv` | Ping-pong weight SRAM (2 shared banks × 32 rows × 32 BF16 columns, used for both forward and backward passes) |
| `bias_sram.sv` | Bias SRAM (32 FP32 values, loaded via AXI tuser=110) |
| `axi_readback.sv` | AXI4-Stream master: streams accumulator rows out with optional bias addition and GELU post-processing; GELU_LATENCY=21 |
| `gelu_unit.sv` | Padé [3/2] GELU approximation, 20-stage FP32 pipeline (one FP32 operation per stage) |
| `output_sram.sv` | Legacy output staging buffer; not instantiated in current `top.sv` |

### `tb/` — Testbenches

| File | Description |
|------|-------------|
| `tb_top.cpp` | Verilator C++ testbench: 17 directed tests covering basic matmul, K-tile accumulation, bias, GELU, backward mode, M=1/M=256 boundaries, ping-pong weight preloading, and drain-overlap LOAD_WT (IDLE→STREAM skip) |
| `tb_gelu_hal.cpp` | HAL-level GELU testbench: 6 tests covering GELU accuracy across 32 input values, multiple readback toggle guard, M_count sweep, bias+GELU, multi K-tile accumulation, and saturation boundaries |

### `sim/` — Simulation Results

| File | Description |
|------|-------------|
| `final_run.log` | Verilator testbench output: all 17 tests pass (`All 32x32 tests PASSED (17/17)`) |
| `final_waveform.png` | GTKWave screenshot showing LOAD_WT → STREAM → DONE state transitions and AXI bus activity |

### `synth/` — Cadence Genus Synthesis Results

Target: ASAP7 predictive 7nm RVT library, 1650 ps clock period (606 MHz target).

| File | Description |
|------|-------------|
| `genus.tcl` | Synthesis script: sets library paths, reads RTL, applies constraints, runs `syn_generic`/`syn_map`/`syn_opt` |
| `constraints.sdc` | Genus timing constraints: 1650 ps clock, input/output delay margins |
| `constraints_pnr.sdc` | Innovus P&R timing constraints: 1700 ps relaxed period, explicit port delay assignments |
| `innovus.tcl` | Cadence Innovus P&R script: floorplan, power rings, placement, CTS, routing, signoff reports |
| `genus.log` | Full Cadence Genus run log |
| `area_report.txt` | Cell-level area breakdown; total 533,441 µm² (0.533 mm²); two accumulator SRAM banks 291,384 µm², systolic array 177,943 µm² |
| `power_report.txt` | Vectorless power estimate: 1.140 W total (register 50.4%, logic 49.6%) |
| `timing_report.txt` | Detailed timing paths; all paths MET; critical path is readback counter → accumulator SRAM read → bias FP32 adder → AXI output, 0 ps slack |
| `qor.rpt` | Quality-of-results summary: WNS = 0 ps, 0 violating paths |

### `bench/` — Benchmark Data

| File | Description |
|------|-------------|
| `benchmark.md` | Formatted benchmark results: CPU vs. Verilator-accelerated timing, energy measurements, roofline analysis, speedup and throughput tables |
| `benchmark_data.txt` | Raw key-value timing data from both software and `systolic_vrl` backends; includes combined projection output (107,807,568 total HW cycles, 25.3× matmul speedup) |
| `roofline_final.png` | Roofline plot showing accelerator operating point |
| `weight_accuracy_final.out` | Weight accuracy comparison (CPU vs. accelerated): 124M parameters, mean absolute difference 9.65×10⁻⁵ |

### `software/` — Hardware Abstraction Layer and Training Code

Snapshot of the software stack used to drive the Verilator simulation and run GPT-2 training benchmarks.

| File | Description |
|------|-------------|
| `accel_hal.h` | HAL public API: lifecycle, `hal_stream_tile`, readback variants (sync and async), bias load, timing, and backward tile interfaces |
| `accel_hal_systolic_vrl.cpp` | Verilator backend HAL: drives `Vsys_top` cycle-by-cycle; implements drain-overlap, concurrent drain-write, async beat-harvesting readback, and `g_accum_gelu_pending` guard |
| `accel_hal_software.c` | Pure-C software simulation backend (no Verilator); used for CPU baseline timing |
| `accel_hal_verilator.cpp` | Legacy single-row vector Verilator backend; superseded by systolic_vrl |
| `train_gpt2.c` | GPT-2 training loop with accelerated matmul calls; implements `accel_matmul`, `accel_matmul_gelu`, `accel_matmul_backward_dinp`, and `accel_matmul_dweight` |
| `accel_hal_systolic_vrl.o` | Compiled object file for the systolic_vrl backend |

### `report/` — Written Report

| File | Description |
|------|-------------|
| `design_justification.pdf` | M4 design justification report covering BF16 precision rationale, architecture decisions, AXI interface, synthesis results, and performance analysis |
| `figures/Block_Design.png` | Block diagram of the full `sys_top` datapath |
| `figures/Roofline.png` | Roofline figure as included in the report |

---

## M4 Checklist Deviations

The M4 checklist is written assuming an OpenLane 2 synthesis flow and a SystemVerilog simulation environment. This project uses Cadence Genus/Innovus and Verilator throughout. The table below maps each expected checklist path to the committed equivalent and explains the reason for the difference.

| Checklist expects | What is committed | Reason |
|---|---|---|
| `synth/config.json` | `synth/genus.tcl` | Cadence Genus was used instead of OpenLane 2. ASAP7 is not natively supported by OpenLane 2, and the PSU lab server runs CentOS 7 with a licensed Cadence installation. `genus.tcl` is the complete synthesis configuration: it sets library search paths, reads all RTL sources, loads `constraints.sdc`, and runs `syn_generic` / `syn_map` / `syn_opt`. |
| `synth/openlane_run.log` | `synth/genus.log` | Full Cadence Genus terminal log. Contains the same information as an OpenLane run log: elaboration messages, timing closure iterations, and the final QoR summary. |
| `tb/tb_top.sv` | `tb/tb_top.cpp` | The testbench is a Verilator C++ driver. Verilator compiles the SystemVerilog RTL (`top.sv` and all submodules) into a C++ model; `tb_top.cpp` instantiates that model, drives inputs cycle-by-cycle, and checks outputs. A `.sv` testbench would require a commercial SV simulator (VCS, Questa) not available in this environment. The testbench is self-contained and runnable from a clean clone with `make run` in `HDL_Systolic_32x32/`. |
| `bench/benchmark_data.csv` | `bench/benchmark_data.txt` | Raw timing data is stored in a key=value flat-text format written directly by `train_gpt2.c`. The file also includes the full combined projection output produced by `combine_timing.py`. Every number in `benchmark.md` traces to a specific key in this file. |

---

## Key Results Summary

| Metric | Value |
|--------|-------|
| Technology | ASAP7 predictive 7nm RVT |
| Clock target | 606 MHz (1650 ps period) |
| PE array | 32×32 = 1,024 BF16 MACs |
| Peak throughput | 1,241 GFLOP/s |
| Effective throughput | 733 GFLOP/s (59.1% PE utilization) |
| Arithmetic intensity (AXI) | 22.5 FLOP/byte (blended across all three matmul passes) |
| Total cell area | 533,441 µm² (0.533 mm²) |
| Accumulator SRAM (2 banks) | 291,384 µm² / 64 KB FP32 |
| Power (vectorless) | 1.140 W |
| WNS | 0 ps (timing closed, 0 violating paths) |
| Critical path | Readback counter → accum SRAM → bias FP32 adder → AXI output |
| Simulation | 17/17 directed tests pass |
| Matmul speedup vs. CPU | 25.3× (Verilator-projected @ 606 MHz) |
| Overall speedup vs. CPU | 5.73× (including non-matmul CPU work) |
| Energy reduction | 444× vs. CPU matmul (Intel RAPL measured) |
