#!/usr/bin/env python3
"""
compare_weights.py — Compare parameter dumps from CPU and accelerator runs.

Usage:
    python3 compare_weights.py <cpu_weights.bin> <accel_weights.bin>
"""

import sys
import numpy as np

CHUNK = 4_000_000   # process 4M floats (~16MB) at a time to stay memory-friendly

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    cpu_path   = sys.argv[1]
    accel_path = sys.argv[2]

    # Memory-map both files — no full load into RAM
    cpu   = np.memmap(cpu_path,   dtype=np.float32, mode='r')
    accel = np.memmap(accel_path, dtype=np.float32, mode='r')

    if cpu.shape != accel.shape:
        print(f"ERROR: size mismatch — cpu={cpu.shape[0]:,}, accel={accel.shape[0]:,}")
        sys.exit(1)

    n = len(cpu)
    print(f"Parameters : {n:,}  ({n*4/1e6:.1f} MB per file)")
    print("Computing statistics in chunks...")

    # ── Single-pass accumulators ───────────────────────────────────────────
    sum_abs_diff   = 0.0
    sum_sq_diff    = 0.0
    max_abs_diff   = 0.0
    sum_rel_diff   = 0.0
    n_rel          = 0
    dot_cc         = 0.0   # cpu · accel
    norm_c_sq      = 0.0   # ||cpu||²
    norm_a_sq      = 0.0   # ||accel||²

    thresholds = [1e-6, 1e-5, 1e-4, 1e-3, 1e-2]
    counts_within = [0] * len(thresholds)

    abs_diffs_sample = []   # collect a sample for median estimation

    for start in range(0, n, CHUNK):
        end  = min(start + CHUNK, n)
        c    = np.array(cpu[start:end],   dtype=np.float64)
        a    = np.array(accel[start:end], dtype=np.float64)
        d    = np.abs(a - c)

        sum_abs_diff  += d.sum()
        sum_sq_diff   += (d ** 2).sum()
        max_abs_diff   = max(max_abs_diff, d.max())
        dot_cc        += np.dot(c, a)
        norm_c_sq     += np.dot(c, c)
        norm_a_sq     += np.dot(a, a)

        # Relative diff on non-tiny weights
        mask = np.abs(c) > 1e-6
        if mask.any():
            sum_rel_diff += (d[mask] / np.abs(c[mask])).sum()
            n_rel        += mask.sum()

        # Threshold counts
        for i, t in enumerate(thresholds):
            counts_within[i] += (d <= t).sum()

        # Collect a small sample for median (first 200K params)
        if start < 200_000:
            abs_diffs_sample.append(d[:min(len(d), 200_000 - start)])

    mean_abs  = sum_abs_diff / n
    std_abs   = np.sqrt(sum_sq_diff / n - mean_abs ** 2)
    median_abs = np.median(np.concatenate(abs_diffs_sample)) if abs_diffs_sample else float('nan')
    cos_sim   = dot_cc / (np.sqrt(norm_c_sq) * np.sqrt(norm_a_sq))

    print()
    print("Absolute difference:")
    print(f"  mean   : {mean_abs:.6e}")
    print(f"  median : {median_abs:.6e}  (estimated from first 200K params)")
    print(f"  max    : {max_abs_diff:.6e}")
    print(f"  std    : {std_abs:.6e}")
    print()

    if n_rel > 0:
        print(f"Relative difference (|cpu| > 1e-6, {n_rel:,} params):")
        print(f"  mean   : {sum_rel_diff/n_rel:.4%}")
        print()

    print(f"Cosine similarity : {cos_sim:.8f}  (1.0 = identical)")
    print()

    print("Fraction of params within threshold:")
    for t, cnt in zip(thresholds, counts_within):
        print(f"  |diff| <= {t:.0e} : {cnt/n:.2%}  ({cnt:,} / {n:,})")

if __name__ == "__main__":
    main()
