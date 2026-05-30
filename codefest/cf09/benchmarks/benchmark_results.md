# Benchmark Results — M4 Accelerator (32×32 BF16 Systolic Array)

**Configuration:** GPT-2 small (12 layers, C=768, 12 attention heads), B=4, T=64.
One complete forward + backward pass. Target process: ASAP7 predictive 7 nm RVT @ 606 MHz (1650 ps).

---

## Performance and Energy Summary

| Metric | Software Baseline | Accelerated (M4) | Improvement |
|---|---|---|---|
| Matmul time | 4.506 s | 0.232 s | **19.4× faster** |
| Total step time | 5.243 s | 0.969 s | **5.41× faster** |
| Matmul power | 19.98 W | 0.900 W | **22.2× lower** |
| Matmul energy | 90.04 J | 0.209 J | **432× lower** |

---

## Notes

**Kernel speedup (19.4×)** is computed as the ratio of CPU matmul wall time to projected
hardware matmul time:

```
kernel_speedup = cpu_matmul_time / hw_matmul_time
               = 4.506 s / 0.232 s
               = 19.4×

hw_matmul_time = hw_cycles / clock_hz
               = 140,424,528 cycles / 606,000,000 Hz
               = 0.2317 s
```

Hardware cycle counts (45,704,328 forward + 94,720,200 backward) are read from a 64-bit
tick counter in the Verilator simulation via `hal_sim_cycle_snapshot()` in
`accel_hal_systolic_vrl.cpp`. CPU timing uses `clock_gettime(CLOCK_MONOTONIC)` on the
software backend (no Verilator involvement), with a warmup pass to eliminate cold-cache
effects.

**Overall speedup (5.41×)** adds non-matmul CPU work (attention, LayerNorm, optimizer)
that is unchanged between the two configurations:

```
overall_speedup = (cpu_matmul_time + cpu_nonmatmul_time)
                / (hw_matmul_time  + cpu_nonmatmul_time)
               = 5.243 s / 0.969 s
               = 5.41×
```

The gap between kernel speedup (19.4×) and overall speedup (5.41×) reflects the
non-matmul fraction (76% of projected runtime): attention softmax, LayerNorm, GELU
backward, and the AdamW optimizer step all remain on the CPU.

**Energy reduction (432×)** is the product of the power reduction and the kernel speedup.
CPU energy is measured via Linux RAPL (`intel-rapl:0`), accumulated over all matmul calls.
Hardware power (0.900 W) is taken from the Genus post-synthesis power report
(`m4/synth/power_report.txt`), ASAP7 RVT active stimulus, and multiplied by the projected
hardware matmul time.
