# Accelerator Benchmark Results

Benchmark configuration: GPT-2 small (12 layers, C=768, 4 heads → 12 attention heads),
B=4, T=64 (batch size 4, sequence length 64). One complete forward + backward pass over
all 12 transformer layers.

---

## Methodology

### CPU Baseline Timing

CPU matmul runtime is measured using the **software backend** — the same C code paths
without any Verilator involvement. This avoids cache pollution from the Verilator JIT and
shared-library overhead, giving a clean CPU-only measurement.

A warmup pass is run first (`ACCEL_SINGLE_PASS` with two iterations, only the second
timed) to eliminate compulsory cache-miss penalties. Timing uses `clock_gettime(CLOCK_MONOTONIC)`.

Energy is measured via the Linux RAPL sysfs interface:
`/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj`. Readings are taken immediately
before and after each matmul call and accumulated. The wraparound value (2⁴⁸ µJ) is
handled explicitly.

### Accelerator (Hardware) Timing

The RTL is simulated cycle-accurately in Verilator. A 64-bit tick counter in the
testbench (`tb_top.cpp`) increments every simulated clock edge. The HAL
(`accel_hal_systolic_vrl.cpp`) reads the counter before and after each accelerated
matmul call and accumulates forward and backward cycle totals separately into the
`hw_cycles_fwd` and `hw_cycles_bwd` fields written to the timing file.

Projected wall time is derived by dividing the cycle counts by the synthesized clock
frequency:

```
hw_time = hw_cycles / clock_hz
clock_hz = 606,000,000  (1650 ps period, ASAP7 7nm RVT constraints)
```

The non-matmul CPU work (attention, layernorm, optimizer) is taken from the software
backend run and added directly, since that work executes on the host CPU regardless of
which backend handles matmul.

### Combined Projection

```
projected_total = hw_matmul_time + cpu_non_matmul_time
speedup         = (cpu_matmul_time + cpu_non_matmul_time) / projected_total
accel_speedup   = cpu_matmul_time / hw_matmul_time
```

Scripts: `combine_timing.py` reads `timing_software.txt` and `timing_systolic_vrl.txt`
and produces the full combined report.

---

## Accelerator Throughput

The 32×32 systolic array performs one MAC per PE per cycle at 606 MHz.

| Metric | Value |
|---|---|
| Peak throughput | 1,024 PEs × 606 MHz × 2 = **1,241 GFLOP/s** |
| Forward cycles | 36,154,760 |
| Backward cycles | 71,652,808 |
| Total cycles | 107,807,568 |
| Projected matmul time | **0.1779 s** |
| Effective throughput | **733 GFLOP/s** |
| PE utilization | **59.1%** |
| Arithmetic intensity (AXI) | **25.5 FLOP/byte** |
| Roofline ceiling (bandwidth-bound) | **989 GFLOP/s** |

Effective throughput is computed as total matmul FLOPs across the full forward +
backward pass divided by projected hardware time:

```
throughput = total_matmul_FLOPs / hw_matmul_time
           = (107,807,568 cycles × 2 FLOPs/cycle × utilization) / 0.1779 s
           ≈ 733 GFLOP/s
```

The remaining utilization gap (40.9% dead cycles) is primarily caused by sequential
weight-load stalls. Each 32-column weight tile requires 32 AXI beats to load into the
SRAM plus 32 LOAD_WT cycles to shift into the PEs before activation streaming can begin.
Both phases run sequentially on the shared AXI-Stream bus, leaving the PE array idle for
a significant fraction of each K-tile iteration. The actual arithmetic intensity at the
AXI bus (25.5 FLOP/byte) also places the kernel just below the ridge point (32
FLOP/byte), making it bandwidth-bound with a ceiling of 989 GFLOP/s even at full bus
utilization.

---

## Speedup Over Software Baseline

| Metric | Software | Accelerated | Ratio |
|---|---|---|---|
| Matmul time | 4.5055 s | 0.1779 s | **25.3×** |
| Non-matmul CPU time | 0.7376 s | 0.7376 s | 1.0× |
| Total projected time | 5.2431 s | 0.9155 s | **5.73×** |
| Accelerator fraction of total | — | 19.4% | — |

The matmul-only speedup of **25.3×** reflects the accelerated portion. The
system-level speedup of **5.73×** is lower because non-matmul work (attention,
layernorm, optimizer) remains on the CPU and accounts for 80.6% of the projected
runtime. By Amdahl's Law, further gains require accelerating those operations or
reducing their share.

---

## Energy Comparison

Hardware energy is estimated from the post-synthesis power report (ASAP7 7nm RVT,
active stimulus, leakage + internal + switching total):

```
hw_power  = 1.140 W  (from cadence/systolic_32x32/reports/power.rpt)
hw_energy = hw_power × hw_matmul_time = 1.140 W × 0.1779 s = 0.2028 J
```

The increase from the prior 0.900 W figure reflects the double-buffered accumulator
SRAM (doubled switching activity) and the 20-stage FP32 GELU pipeline replacing the
earlier 8-stage BF16 pipeline.

CPU energy is measured by RAPL (package-level, accumulated over all matmul calls
across the timed training step):

```
cpu_energy = 90.040 J  (RAPL package counter, software backend)
cpu_power  = 90.040 J / 4.5055 s = 19.98 W
```

| Metric | CPU (software) | Accelerator (projected) | Ratio |
|---|---|---|---|
| Matmul time | 4.5055 s | 0.1779 s | 25.3× faster |
| Power (matmul portion) | 19.98 W | 1.140 W | **17.5× lower** |
| Energy (matmul portion) | 90.040 J | 0.2028 J | **444× lower** |

The energy reduction (444×) is the product of the power reduction (17.5×) and the
speedup (25.3×).

---

## Numerical Accuracy

Comparison of learned weight tensors after one training step between the software baseline
and the accelerated (systolic array) run. 124,475,904 parameters total (497.9 MB per file).

| Metric | Value |
|---|---|
| Cosine similarity | 0.99999951 (1.0 = identical) |
| Mean relative difference | 0.8011% |
| Mean absolute difference | 9.65 × 10⁻⁵ |
| Max absolute difference | 2.00 × 10⁻⁴ |
| Params within 1 × 10⁻³ | 100.00% |
| Params within 1 × 10⁻⁴ | 51.94% |

All 13 unit tests in the Verilator testbench pass, including identity-weight regression,
accumulator drain, and multi-tile tiling tests.

