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
