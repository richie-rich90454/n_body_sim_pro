---
title: SIMD
description: The SIMD coverage matrix, runtime dispatch, the AVX2 all-pairs kernel, the experimental SIMD Barnes-Hut kernel, and correctness testing.
---

# SIMD

## Coverage matrix

A backend is only **selected** when a real kernel exists for it. The
detected ISA and the selected backend are distinct facts, reported
separately in the UI and `hardware`.

| Backend | Detection | Kernel status |
|---------|-----------|---------------|
| Scalar | always | reference kernel (correctness authority) |
| SSE2 | CPUID | not yet implemented; falls back to scalar |
| AVX2 + FMA | CPUID | all-pairs force kernel, 256-bit lanes |
| AVX-512 | CPUID | not yet implemented; never selected |
| NEON | ARM64 (always present) | not yet implemented; never selected |

On the development machine the selected backend is **AVX2**. On a machine
without AVX2+FMA it is `scalar`, until the other kernels exist.

## Runtime dispatch

At startup the engine detects CPU features and selects the best backend for
which a real kernel exists. The AVX2 kernels live in their own translation
units compiled with `-mavx2 -mfma`, so the rest of the engine does not
require the ISA.

## AVX2 all-pairs kernel

The inner loop over source particles is vectorized four-at-a-time: all three
coordinate deltas, the distance, and the softened inverse-distance-cubed are
computed in 256-bit lanes, and the acceleration accumulation uses fused
multiply-add (`_mm256_fmadd_pd`).

Two correctness details:

1. **Self-interaction**: a particle's own lane contributes zero force because
   its delta is exactly zero. With zero softening that lane would evaluate
   `0 × ∞ = NaN`, so when softening is zero the self lane's distance-squared
   is blended to 1.0, keeping the contribution exactly zero.
2. **Tolerance, not bit equality**: lane-wise accumulation reorders the sum
   versus the reference, so results agree within 1e-10 relative, not
   bit-for-bit.

Measured (16,384 particles, 1 thread): **4.6×** over the scalar reference.

## SIMD Barnes-Hut

A SIMD Barnes-Hut kernel is implemented (`barnes_hut_avx2.c`). It mirrors
the scalar walk's acceptance decisions exactly and stages the accepted
interactions into four SIMD slots, flushing them into register accumulators
with 256-bit FMA. Because the acceptance decisions are identical, the result
matches the scalar Barnes-Hut kernel to **1e-9 relative** — only the
floating-point summation order differs.

Measured on the development machine, it is **not faster** than the scalar
kernel:

| N (1 thread) | scalar Barnes-Hut | SIMD Barnes-Hut |
|--------------|-------------------|-----------------|
| 65,536 | 68 ms | 300 ms |
| 262,144 | 234 ms | 1,643 ms |

The traversal is memory-latency bound: each particle walks ~30–60 tree nodes
with random access, and vectorizing the per-interaction arithmetic adds
staging overhead without removing the cache misses. The scalar kernel is the
default; the SIMD variant is exposed for experimentation (the UI labels it
"experimental") and is documented honestly rather than claimed as an
optimization.

Three designs were benchmarked before settling on the register-staged one:

| design | 262,144 result |
|--------|----------------|
| group-of-4 descent | 2× slower |
| large batch buffer | 6.7× slower |
| register staging | ~7× slower (still slower than scalar) |

Each was measured, compared against scalar, and the slow ones discarded or
reworked. The specification's rule — benchmark, then keep or revert, and
document — was followed for all of them.

## Correctness testing

`tests/numerical/simd_equivalence_test.c` runs every implemented SIMD kernel
against the reference on identical inputs and asserts the relative error is
below tolerance, both with and without softening. It is part of the default
CTest suite, so a regression in any backend fails locally.
