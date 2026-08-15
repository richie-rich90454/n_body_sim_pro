# SIMD

## Kernel coverage matrix

A SIMD backend is only *selected* when a real kernel exists for it. The
detected ISA and the selected backend are distinct facts and are reported
separately in the UI and `hardware` command.

| Backend | Detection | Kernel status |
|---------|-----------|---------------|
| Scalar | always | reference kernel (correctness authority) |
| SSE2 | CPUID | not yet implemented; falls back to scalar |
| AVX2 + FMA | CPUID | all-pairs force kernel, 256-bit lanes |
| AVX-512 | CPUID | not yet implemented; never selected |
| NEON | ARM64 (always present) | not yet implemented; never selected |

The selected backend is reported honestly: on this project's development
machine it is `AVX2`; on a machine without AVX2+FMA it would be `scalar`
until the other kernels exist.

## AVX2 force kernel

The kernel vectorizes the inner loop over source particles: four at a time,
all three coordinate deltas, the distance, and the softened
inverse-distance-cubed are computed in 256-bit lanes, and the acceleration
accumulation uses fused multiply-add (`_mm256_fmadd_pd`). It is compiled
with `-mavx2 -mfma` as a single translation unit, so the rest of the engine
does not require the ISA.

Two correctness details:

1. **Self-interaction.** A particle's own lane contributes zero force
   because its delta is exactly zero. With zero softening that lane would
   evaluate `0 * inf = NaN`, so when softening is zero the self lane's
   distance-squared is blended to 1.0, keeping the contribution exactly zero.
2. **Tolerance, not bit equality.** Lane-wise accumulation reorders the sum
   versus the reference, so results agree within floating-point tolerance
   (validated at 1e-10 relative in the test suite).

## Correctness testing

`tests/numerical/simd_equivalence_test.c` runs every implemented SIMD kernel
against the reference on the same inputs and asserts the relative error is
below tolerance, both with and without softening. It is part of the default
test suite, so a regression in any backend fails CI-style runs locally.

## SIMD Barnes-Hut

A SIMD Barnes-Hut force kernel is implemented (`barnes_hut_avx2.c`) and
validated against the scalar kernel to 1e-9 relative (identical acceptance
decisions; only the floating-point summation order differs). Its traversal
mirrors the scalar walk exactly and stages the accepted interactions into
four SIMD slots, flushing them into register accumulators with 256-bit FMA.

Measured on the development machine (Intel Core Ultra 7 255H), the SIMD
kernel is **not** faster than the scalar one:

| N (1 thread) | scalar Barnes-Hut | SIMD Barnes-Hut |
|--------------|-------------------|-----------------|
| 65,536 | 68 ms | 300 ms |
| 262,144 | 234 ms | 1,643 ms |

The traversal is memory-latency bound: each particle walks ~30-60 tree nodes
with random access, and vectorizing the per-interaction arithmetic adds
staging overhead without removing the cache misses. The scalar kernel is the
default; the SIMD variant is available for experimentation and for hardware
where the arithmetic share of the cost is larger. These numbers are reported
rather than hidden: an optimization that does not measure faster is not
claimed as one.
