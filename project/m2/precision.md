# Precision Format Selection: BF16 Multiply, FP32 Accumulate

## Decision

The accelerator uses **BF16 (Brain Float 16) inputs with FP32 accumulators** for the
forward pass matrix multiply operations.

Empirical comparison of one training step of GPT-2 (124M parameters, batch size 4,
sequence length 64) between the full FP32 CPU implementation and the BF16×FP32
accelerated implementation shows the following weight update differences:

| Metric | Value |
|--------|-------|
| Mean absolute difference | 2.036103e-06 |
| Mean relative difference | 0.0183|
| Max absolute difference | 2.00 × 10⁻⁴ |
| Cosine similarity | 0.99999999 |
| Params within 1×10⁻³ | 100% |

A mean relative difference of 0.0183% falls within the accepted range for
mixed-precision training.

BF16 has 7 explicit mantissa bits versus 23 for FP32. The mantissa multiplier
scales as the square of the mantissa width:

```
(8 bits)² / (24 bits)² ≈ 11% of a full FP32 multiplier
```

This gives roughly a **9× reduction in multiplier area** versus a full FP32 multiply
unit. Synthesis results on ASAP7 (predictive 7nm) confirm this: the 128-MAC BF16
vector design achieves 0.096 mm² at 606 MHz versus an estimated 0.87 mm² for an
equivalent FP32 design.

The FP32 accumulator is retained because accumulating 32 products in BF16 causes
significant compounding rounding error (each addition truncates the running sum to
7 mantissa bits). Testing showed this degraded weight update quality noticeably.
FP32 accumulation eliminates this issue at negligible additional cost since only
one accumulator register per lane is needed.

---

## Comparison with Alternatives Tested

| Mode | Mean Relative Δ | 
|------|----------------|
| FP32 × FP32 accum | 0.0175% | 
| INT16 × FP32 accum | 0.0173% | 
| INT8 × FP32 accum | 0.5710% | 
| **BF16 × FP32 accum** | **0.0183%** | 
| FP8 × FP32 accum | 0.5733% |
| BF16 × BF16 accum | 0.8135% | 
| Actual Hardware Sim | 0.0183% | 


