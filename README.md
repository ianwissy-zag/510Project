# Hardware Acceleration of GPT-2 Training

Ian Wyse — ECE 510 Spring 2026

## Project Overview

This project implements and evaluates custom ASIC accelerators for the matrix multiply (GEMM) operations that dominate GPT-2 training. Three accelerator architectures are developed, all targeting the ASAP7 predictive 7nm PDK via the Cadence Genus/Innovus toolchain. The designs are integrated with a modified GPT-2 C training loop (`Source/llm_accel/`) that replaces the CPU matmul with accelerator calls through a hardware abstraction layer supporting software simulation and Verilator-backed cycle-accurate simulation.

---

## Precision Justification: BF16

All three accelerators use **BF16 (Brain Float 16)** inputs with **FP32 accumulators**, matching the mixed-precision training approach used in production ML hardware (NVIDIA tensor cores, Google TPU).

Due to the more stringent accuracy requirements of training compared to precision, lower precision types like INT8 or BF8 were not 
considered. Testing has found that BF16 implementations of my algorithm have a single step relative difference of .0175%, which is entirely acceptable. This reduction in precision meant that multipliers were 9x smaller than full FP32 multipliers as the size goes as the number of mantissa bits squared. This also resulted in the accumulate, rather than the multiply step dominating critical path timing, a sign that my implementation was resulting in speedup. 

**The result:** BF16 multiply + FP32 accumulate preserves the dynamic range needed for training while delivering significant area and power savings over full FP32 arithmetic.

---

## Accelerator Architectures

### 1. Systolic Array (HDL/)

| Property | Value |
|----------|-------|
| Architecture | 32×32 weight-stationary systolic array |
| MAC units | 1,024 (32 rows × 32 columns) |
| Data path | BF16 inputs → FP32 accumulation |
| Output channels | 32 per cycle (one column) |
| Interface | AXI4-Stream (slave for weights/activations, master for readback) |
| FPU | Berkeley HardFloat (blackboxed for synthesis) |
| Development state | **RTL simulation verified** (Verilator); synthesis pending |

**Architecture notes:** Weight-stationary design. Weights are loaded into PE registers during a LOAD_WT phase, then activations flow diagonally through the array. The blackbox strategy is used for synthesis: the 32×32 PE mesh is represented as a LEF macro with correct port geometry, while the surrounding AXI, SRAM, controller, and readback logic is fully synthesized in sky130A via OpenLane.

**Throughput limitation:** The LOAD_WT phase (32+ idle cycles before each tile) and diagonal wavefront fill/drain reduce effective utilization to approximately 34%. Throughput at achievable clock: estimated ~50 GFLOPS pending synthesis completion.

---

### 2. Vector MAC — HardFloat FPU (HDL_Vect/)

| Property | Value |
|----------|-------|
| Architecture | 128-wide parallel vector MAC |
| MAC units | 128 |
| Data path | BF16 inputs → FP32 accumulation |
| Output channels | 128 per tile |
| Interface | AXI4-Stream (512-bit beats; 4 beats/weight row, 1 beat/activation, 8 beats/readback) |
| FPU | Berkeley HardFloat (expWidth=8, sigWidth=24) |
| Development state | **RTL simulation verified** (Verilator); **synthesis complete** (Cadence Genus, ASAP7); P&R in progress (Cadence Innovus) |
| Target clock | 1,750 ps → **~571 MHz** |
| Cell count | 1,107,533 |
| Cell area | 120,054 µm² (0.12 mm²) |
| Total power | 4.59 W (vectorless estimate) |
| Throughput | 128 MACs × 571 MHz = **~73 GFLOPS** |

**Architecture notes:** No LOAD_WT phase — weight rows stream directly from SRAM to the 128 MAC units each compute cycle, giving close to 100% utilization. Ping-pong buffering (2× weight SRAM banks, 2× activation banks) enables double-buffering across tiles. K_DEPTH=32 inner products per tile. The HardFloat core performs a full FP32 multiply internally (BF16 inputs are zero-extended), which is correct but uses a 24×24 bit mantissa multiplier even though BF16 inputs only have 8 significant mantissa bits.

---

### 3. Vector MAC — Custom BF16 FPU (HDL_Vect_BF16/)

| Property | Value |
|----------|-------|
| Architecture | 128-wide parallel vector MAC |
| MAC units | 128 |
| Data path | BF16 inputs → FP32 accumulation |
| Output channels | 128 per tile |
| Interface | AXI4-Stream (identical to HardFloat version) |
| FPU | Custom: `bf16_mul.sv` (8×8 mantissa multiply) + `bf16_fp32_add.sv` (FP32 adder) |
| Development state | **RTL simulation verified** (Verilator); **synthesis complete** (Cadence Genus, ASAP7); P&R in progress (Cadence Innovus) |
| Target clock | 1,650 ps → **~606 MHz** |
| Cell count | 775,391 (−30% vs HardFloat) |
| Cell area | 96,253 µm² (0.096 mm²) — **−20% vs HardFloat** |
| Total power | 3.27 W — **−29% vs HardFloat** |
| Throughput | 128 MACs × 606 MHz = **~78 GFLOPS** |

**Architecture notes:** Identical AXI interface and tiling scheme to the HardFloat version. The key difference is the FPU: `bf16_mul` performs an 8×8 mantissa multiply (exploiting the fact that BF16 inputs have only 7 explicit mantissa bits + 1 implicit), producing a result that fits exactly in FP32's mantissa field with no rounding. The FP32 adder (`bf16_fp32_add`) accumulates the product into the running partial sum. This eliminates the unnecessary 24×24 multiply of the HardFloat path.

**Critical path analysis:** The FP32 adder accounts for 53% of the critical path (approximately 830 ps), the multiplier 28% (~465 ps), and controller/broadcast logic 17% (~268 ps). The adder's carry-propagation chain is the primary timing bottleneck regardless of multiplier implementation. Pipelining the MAC (splitting multiply and add into separate clock stages) would roughly halve the critical path depth and is the natural next step.

**Numerical accuracy:** Software co-simulation against the CPU reference (one training step of GPT-2) yields 0.0175% mean relative weight error — two orders of magnitude below the ~1% threshold where training convergence is affected.

---

## AXI4-Stream Interface

All three designs share a common AXI4-Stream interface convention:

| Signal | Direction | Purpose |
|--------|-----------|---------|
| `s_axis_tdata` | In | 512-bit data (weights or activations) |
| `s_axis_tuser` | In | 2-bit routing: 00=wt ping, 01=wt pong, 10=act ping, 11=act pong |
| `s_axis_tvalid` | In | Data valid |
| `s_axis_tready` | Out | Ready to accept |
| `s_axis_tlast` | In | Last beat of burst |
| `m_axis_tdata` | Out | 512-bit readback data (FP32 psums) |
| `m_axis_tvalid` | Out | Readback data valid |
| `m_axis_tready` | In | Host ready to receive |
| `m_axis_tlast` | Out | Last readback beat |

AXI4-Stream was chosen over memory-mapped interfaces because the accelerator's data flow is inherently streaming: weights and activations are produced and consumed in order with no random access required. The 512-bit bus width amortizes protocol overhead across 32 BF16 values per beat, giving high bandwidth utilization at modest clock frequencies.

---

## Software Integration

`Source/llm_accel/train_gpt2.c` replaces the CPU `matmul_forward` with an accelerator-backed implementation. The software stack has three layers:

- **Tiling engine** — decomposes arbitrary M×K×N matmul into ACCEL_VEC_SIZE=128 × ACCEL_K_DEPTH=32 tiles
- **HAL** — `accel_hal_software.c` (fast, pure-C BF16 simulation) or `accel_hal_verilator.cpp` (cycle-accurate Verilator simulation)
- **Projected timing** — `accel_print_timing()` reports estimated hardware wall time based on tile count and clock frequency from synthesis

Build targets:
```
make BACKEND=software SINGLE_PASS=1   # software simulation, one step
make BACKEND=vect128  SINGLE_PASS=1   # Verilator + HardFloat FPU
make BACKEND=bf16     SINGLE_PASS=1   # Verilator + custom BF16 FPU
```

Weight comparison between CPU and accelerated runs:
```
python3 Source/compare_weights.py Source/llm/weights_cpu.bin Source/llm_accel/weights_accel.bin
```
