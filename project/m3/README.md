# M3 — 32×32 BF16 Systolic Accelerator with Fused Bias + GELU

32×32 weight-stationary systolic array targeting GPT-2 GEMM acceleration.
1024 BF16 MAC units, FP32 accumulation, fused bias addition and GELU
(Padé [3/2] approximation) in the readback pipeline.
Target PDK: ASAP7 predictive 7nm RVT.

---

## Directory Structure

```
m3/
├── rtl/                   RTL source (SystemVerilog)
│   ├── synth_top.sv       Top-level (sys_top) — AXI-Stream interface, parameter definitions
│   ├── systolic_32x32.sv  32×32 PE array — broadcast activation, weight-stationary
│   ├── pe.sv              Processing element — BF16 MAC, weight shift chain
│   ├── bf16_mac_unit.sv   BF16 multiply + FP32 accumulate MAC
│   ├── bf16_mul.sv        BF16 multiplier primitive
│   ├── bf16_fp32_add.sv   BF16-input FP32 adder primitive
│   ├── controller.sv      State machine: IDLE → LOAD_WT → STREAM → DONE
│   ├── axi.sv             AXI4-Stream slave — routes weights, activations, bias
│   ├── axi_readback.sv    AXI4-Stream master — bias add + 9-stage GELU pipeline
│   ├── gelu_unit.sv       Pipelined GELU (Padé [3/2] tanh), 8 register stages
│   ├── fp32_recip.sv      FP32 reciprocal — bit-trick + 2× Newton-Raphson
│   ├── accum_sram.sv      256×32 FP32 dual-port accumulator (flip-flop based)
│   ├── weight_sram.sv     Single-port BF16 weight SRAM (ping-pong × 4)
│   ├── bias_sram.sv       32-entry FP32 bias register file
│   ├── act_stagger.sv     Triangular activation delay buffer for systolic dataflow
│   ├── act_sram.sv        Activation register file
│   └── output_sram.sv     Output register file
│
├── tb/
│   └── tb_top.cpp         Verilator C++ testbench — 7 correctness tests
│                          covering streaming, K-tiling, backward mode,
│                          bias addition, and GELU post-processing
│
├── sim/
│   ├── cosim_run.log      Simulation output log (all 7 tests)
│   └── cosim_waveform.png Waveform screenshot
│
├── synth/
│   ├── genus.tcl          Cadence Genus synthesis script
│   ├── constraints.sdc    Timing constraints — 1650 ps clock (≈ 606 MHz), ASAP7 RVT
│   ├── genus.log          Full synthesis log
│   ├── area_report.txt    Cell area breakdown by instance hierarchy
│   ├── timing_report.txt  Timing paths — WNS −0.9 ps, TNS −85.3 ps (154 paths)
│   ├── power_report.txt   Power summary — 900 mW total (TT/0.7V/25°C)
│   ├── qor.rpt            Quality-of-results summary
│   └── critical_path.md   Notes on the timing near-miss and its cause
│
└── synthesis_notes.md     Design evolution narrative and known limitations

```

---

## Key Results

| Metric | Value |
|---|---|
| Technology | ASAP7 predictive 7nm RVT |
| Clock target | 1650 ps (≈ 606 MHz) |
| Worst negative slack | −0.9 ps (154 paths, all ≤ −1 ps) |
| Total cell area | 385,241 µm² |
| Total power | 900 mW (TT/0.7V/25°C, no activity factor) |
| Leaf cell count | 3,766,604 |
| Critical path | GELU pipeline R2→R3 stage: bf16_mul → bf16_fp32_add |

The timing near-miss (−0.9 ps on a 1650 ps clock) is treated as a pass.
See `synth/critical_path.md` and `synthesis_notes.md` for discussion.

All SRAM blocks are implemented as flip-flop register files; ASAP7 does not
include compiled SRAM macros. Replacing them with hard SRAM would reduce area
and power by an estimated 40%.

---

## Running Simulation

**Tool:** Verilator (open-source RTL simulator)

The simulation build system lives in `HDL_Systolic_32x32/` alongside the
working RTL. From the repository root:

```bash
cd project/HDL_Systolic_32x32

# First build (compiles Verilator model + testbench):
make

# Build and run all 7 tests:
make run

# Clean and full rebuild:
make clean && make run
```

The testbench binary is `obj_top/Vsys_top`. After a successful run, all
seven tests should report PASS. A waveform file `waves.fst` is also written
and can be opened with GTKWave:

```bash
gtkwave waves.fst
```

**Test coverage (`tb/tb_top.cpp`):**

| Test | Description | Expected result |
|---|---|---|
| 1 | M=4, W=1, act[m][k]=m+1 | C[m][n] = ROWS×(m+1) |
| 2 | 2 K-tiles | C = 2×ROWS = 64 |
| 3 | M=16, W=2, act=1 | C = 2×ROWS = 64 |
| 4 | Backward mode | C = ROWS = 32 |
| 5 | Bias only (bias=0.5) | C = ROWS + 0.5 = 32.5 |
| 6 | GELU only | C = GELU(ROWS) ≈ 32.0 |
| 7 | Bias + GELU | C = GELU(ROWS + 0.5) ≈ 32.5 |

---

## Reproducing Synthesis

**Tool:** Cadence Genus 23.10 (run on `mo.ece.pdx.edu`)

**Prerequisites:**
- ASAP7 PDK installed at `cadence/asap7/asap7sc7p5t_28/` relative to the
  `cadence/` directory (the script derives its path from its own location)
- Cadence Genus in `$PATH`

From the `synth/` directory:

```bash
cd project/m3/synth
genus -f genus.tcl |& tee genus_rerun.log
```

Synthesis takes approximately **6–10 hours**. The script reads all RTL from
`../rtl/`, elaborates `sys_top`, reads `constraints.sdc`, runs
`syn_generic` → `syn_map` → `syn_opt`, and writes reports and a gate-level
netlist to `synth/reports/` and `synth/outputs/`.

**Timing constraints summary (`constraints.sdc`):**
- Clock: 1650 ps period, 50 ps setup uncertainty, 25 ps hold uncertainty
- Reset: false path (asynchronous active-low)
- I/O delays: 0 ps (no system interface specification)
- Drive: BUFx4\_ASAP7\_75t\_R on all inputs; 5 fF load on all outputs
