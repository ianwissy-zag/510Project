#!/usr/bin/env python3
"""
combine_timing.py — Produce an accurate projected runtime by combining:
  - CPU non-matmul time from a software/systolic_sw backend run
    (no Verilator → no cache pollution → reliable CPU timing)
  - Hardware cycle count from a systolic_vrl Verilator run
    (cycle-accurate tick counter, divided by clock frequency)

Usage:
  python3 combine_timing.py [cpu_timing_file] [vrl_timing_file]

Defaults:
  cpu_timing_file : timing_software.txt
  vrl_timing_file : timing_systolic_vrl.txt

Both files are written automatically by train_gpt2 at end of run.
"""

import sys
import os

# Synthesis power for the 32×32 systolic array (ASAP7 7nm RVT, 606 MHz).
# Source: m4/synth/power_report.txt — register + logic total, active stimulus.
HW_SYNTH_POWER_W = 0.900371


def read_timing(path):
    d = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if '=' in line:
                k, v = line.split('=', 1)
                d[k.strip()] = v.strip()
    return d


def get_float(d, key, default=0.0):
    return float(d.get(key, default))


def get_int(d, key, default=0):
    return int(d.get(key, default))


def main():
    cpu_file = sys.argv[1] if len(sys.argv) > 1 else 'timing_software.txt'
    vrl_file = sys.argv[2] if len(sys.argv) > 2 else 'timing_systolic_vrl.txt'

    if not os.path.exists(cpu_file):
        print(f"ERROR: CPU timing file not found: {cpu_file}")
        print("  Run train_gpt2 with software or systolic_sw backend to generate it.")
        sys.exit(1)

    if not os.path.exists(vrl_file):
        print(f"ERROR: VRL timing file not found: {vrl_file}")
        print("  Run train_gpt2 with systolic_vrl backend to generate it.")
        sys.exit(1)

    cpu = read_timing(cpu_file)
    vrl = read_timing(vrl_file)

    # ── CPU non-matmul time (from software/sw run, no cache pollution) ────────
    cpu_fwd      = get_float(cpu, 'fwd_total_s')
    cpu_bwd      = get_float(cpu, 'bwd_total_s')
    cpu_other    = get_float(cpu, 'other_s')
    cpu_matmul   = get_float(cpu, 'matmul_s')
    cpu_attn     = get_float(cpu, 'attention_s')
    cpu_ln       = get_float(cpu, 'layernorm_s')
    cpu_gelu_bwd = get_float(cpu, 'gelu_bwd_s')

    cpu_non_matmul = cpu_fwd + cpu_bwd + cpu_other - cpu_matmul
    cpu_bucket_other = cpu_non_matmul - cpu_attn - cpu_ln - cpu_gelu_bwd

    # ── Hardware projected time (from VRL run, cycle-accurate) ────────────────
    hw_fwd_cycles = get_int(vrl, 'hw_cycles_fwd')
    hw_bwd_cycles = get_int(vrl, 'hw_cycles_bwd')
    clock_hz      = get_float(vrl, 'clock_hz', 606e6)

    hw_fwd_s   = hw_fwd_cycles / clock_hz
    hw_bwd_s   = hw_bwd_cycles / clock_hz
    hw_total_s = hw_fwd_s + hw_bwd_s

    projected_total = hw_total_s + cpu_non_matmul

    B = cpu.get('B', vrl.get('B', '?'))
    T = cpu.get('T', vrl.get('T', '?'))

    print(f"\n=== Combined timing projection (B={B} T={T}) ===")
    print(f"  Sources:")
    print(f"    CPU timing : {cpu_file}  (backend={cpu.get('backend','?')})")
    print(f"    VRL timing : {vrl_file}  (backend={vrl.get('backend','?')})")
    print()
    print(f"  ── Projected hardware (@ {clock_hz/1e6:.0f} MHz) ────────────────")
    print(f"  Forward  matmul + GELU    : {hw_fwd_cycles:>12,d} cycles → {hw_fwd_s:.4f} s")
    print(f"  Backward dinp + dweight   : {hw_bwd_cycles:>12,d} cycles → {hw_bwd_s:.4f} s")
    print(f"  Total hardware            :               {hw_total_s:.4f} s")
    print()
    print(f"  ── CPU wall time (non-matmul, from {cpu.get('backend','?')} run) ─")
    print(f"  attention fwd + bwd       : {cpu_attn:.4f} s")
    print(f"  layernorm fwd + bwd       : {cpu_ln:.4f} s")
    print(f"  gelu_backward             : {cpu_gelu_bwd:.4f} s")
    print(f"  optimizer + other         : {cpu_bucket_other:.4f} s")
    print(f"  Total CPU (non-matmul)    : {cpu_non_matmul:.4f} s")
    print(f"  [CPU matmul ref time      : {cpu_matmul:.4f} s]")
    print()
    print(f"  ── Projected system total ───────────────────────────")
    print(f"  Total (hw + CPU)          : {projected_total:.4f} s")
    if projected_total > 0:
        print(f"  Accel fraction            : {100*hw_total_s/projected_total:.1f}%")
        print(f"  CPU fraction              : {100*cpu_non_matmul/projected_total:.1f}%")
        print(f"  Speedup vs CPU-only       : {(cpu_matmul + cpu_non_matmul)/projected_total:.2f}×")
    print()
    print(f"  ── Accelerated-portion speedup (matmul only) ────────")
    if hw_total_s > 0:
        accel_speedup = cpu_matmul / hw_total_s
        print(f"  CPU matmul time           : {cpu_matmul:.4f} s  (from {cpu.get('backend','?')} run)")
        print(f"  HW matmul time (projected): {hw_total_s:.4f} s  ({hw_fwd_cycles+hw_bwd_cycles:,d} cycles @ {clock_hz/1e6:.0f} MHz)")
        print(f"  Speedup on accel portion  : {accel_speedup:.2f}×")
    else:
        print(f"  (hw_total_s = 0, cannot compute accelerated-portion speedup)")
    print()
    print(f"  ── Power & energy (matmul portion) ──────────────────")
    cpu_energy_j = get_float(cpu, 'matmul_energy_j', -1.0)
    hw_energy_j  = HW_SYNTH_POWER_W * hw_total_s if hw_total_s > 0 else 0.0

    if cpu_energy_j > 0 and cpu_matmul > 0:
        cpu_avg_power_w = cpu_energy_j / cpu_matmul
        print(f"  CPU matmul energy (RAPL)  : {cpu_energy_j:.3f} J")
        print(f"  CPU avg power (matmul)    : {cpu_avg_power_w:.2f} W")
    else:
        print(f"  CPU matmul energy         : not available")
        print(f"  (re-run software backend after: sudo chmod o+r /sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj)")

    if hw_total_s > 0:
        print(f"  HW matmul energy (synth)  : {hw_energy_j:.4f} J"
              f"  ({HW_SYNTH_POWER_W:.3f} W × {hw_total_s:.4f} s)")

    if cpu_energy_j > 0 and hw_energy_j > 0:
        power_reduction  = cpu_avg_power_w / HW_SYNTH_POWER_W
        energy_reduction = cpu_energy_j    / hw_energy_j
        print()
        print(f"  ── Energy cost reduction ────────────────────────────")
        print(f"  Power reduction           : {power_reduction:.1f}×"
              f"  ({cpu_avg_power_w:.1f} W → {HW_SYNTH_POWER_W:.3f} W)")
        print(f"  Energy reduction          : {energy_reduction:.0f}×"
              f"  ({cpu_energy_j:.1f} J → {hw_energy_j:.4f} J)")
        print(f"  = {power_reduction:.1f}× lower power  ×  {cpu_matmul/hw_total_s:.1f}× speedup")
    print(f"================================================\n")


if __name__ == '__main__':
    main()
