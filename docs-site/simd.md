---
title: SIMD
description: The SIMD coverage matrix, runtime dispatch, the AVX2 / AVX-512 / NEON all-pairs kernels, the SIMD Barnes-Hut kernels, the SIMD distributed traversal, and correctness testing.
---

# SIMD

## Coverage matrix

A backend is only **selected** when a real kernel exists for it. The
detected ISA and the selected backend are distinct facts, reported
separately in the UI and `hardware`.

| Backend | Detection | Kernel status |
|---------|-----------|---------------|
| Scalar | always | reference kernel (correctness authority) |
| SSE2 | CPUID | detected, but no SSE2-specific kernel; SSE2 stays scalar |
| AVX2 + FMA | CPUID | all-pairs and Barnes-Hut kernels, 256-bit lanes |
| AVX-512 | CPUID | all-pairs and Barnes-Hut kernels, 512-bit lanes |
| NEON | ARM64 (always present) | all-pairs and Barnes-Hut kernels, 128-bit lanes |

On the development machine the selected backend is **AVX2** (the CPU has no
AVX-512). On a machine with AVX-512 the selected backend is **AVX-512**; on an
ARM64 machine it is **NEON**. Preference order is AVX-512, AVX2, NEON, scalar,
and CMake defines `N_BODY_SIM_PRO_HAVE_*_KERNEL` only when the matching
translation unit is built with the ISA's flags, so a backend is never selected
on a build that lacks its kernel.

## Runtime dispatch

At startup the engine detects CPU features and selects the best backend for
which a real kernel exists. Each SIMD kernel lives in its own translation
unit compiled with the matching ISA flags (`-mavx2 -mfma`, `-mavx512f -mfma`,
or NEON's baseline), so the rest of the engine does not require the ISA.

## All-pairs kernels

The inner loop over source particles is vectorized —four at a time for AVX2
(256-bit), eight for AVX-512 (512-bit), two for NEON (128-bit): the coordinate
deltas, the distance, and the softened inverse-distance-cubed are computed in
vector lanes, and the accumulation uses fused multiply-add.

Two correctness details apply to every kernel:

1. **Self-interaction**: a particle's own lane contributes zero force because
   its delta is exactly zero. With zero softening that lane would evaluate
   `0 × ∞ = NaN`, so when softening is zero the self lane's distance-squared
   is blended to 1.0, keeping the contribution exactly zero.
2. **Tolerance, not bit equality**: lane-wise accumulation reorders the sum
   versus the reference, so results agree within 1e-10 relative, not
   bit-for-bit.

Measured on the development machine (16,384 particles, 1 thread): the AVX2
kernel is **4.6×** over the scalar reference. The AVX-512 and NEON kernels are
not measured on this machine (it has no AVX-512 and is not ARM64).

## SIMD Barnes-Hut

SIMD Barnes-Hut kernels are implemented for AVX2, AVX-512, and NEON
(`barnes_hut_avx2.c`, `barnes_hut_avx512.c`, `barnes_hut_neon.c`). They mirror
the scalar walk's acceptance decisions exactly and stage the accepted
interactions into SIMD slots (4, 8, or 2), flushing them into register
accumulators with FMA. Because the acceptance decisions are identical, the
result matches the scalar Barnes-Hut kernel to **1e-9 relative** — only the
floating-point summation order differs.

Measured on the development machine (AVX2), the SIMD kernel is **not faster**
than the scalar kernel:

| N (1 thread) | scalar Barnes-Hut | SIMD Barnes-Hut (AVX2) |
|--------------|-------------------|------------------------|
| 65,536 | 68 ms | 300 ms |
| 262,144 | 234 ms | 1,643 ms |

The traversal is memory-latency bound: each particle walks ~30–60 tree nodes
with random access, and vectorizing the per-interaction arithmetic adds
staging overhead without removing the cache misses. The scalar kernel is the
default; the SIMD variants are exposed for experimentation (the UI labels them
"experimental") and are documented honestly rather than claimed as an
optimization.

Three AVX2 designs were benchmarked before settling on the register-staged
one:

| design | 262,144 result |
|--------|----------------|
| group-of-4 descent | 2× slower |
| large batch buffer | 6.7× slower |
| register staging | ~7× slower (still slower than scalar) |

Each was measured, compared against scalar, and the slow ones discarded or
reworked. The specification's rule — benchmark, then keep or revert, and
document — was followed for all of them.

## SIMD distributed traversal

The distributed Barnes-Hut traversal is SIMD-accelerated too
(`distributed_barnes_hut_avx2.c`, `_avx512.c`, `_neon.c`). The
essential-tree exchange and the local tree build are byte-identical to the
scalar distributed kernel; only the per-particle force accumulation over the
local tree and the remote essential forest is staged and applied with vector
FMA. The opening decisions are identical, so the result matches the scalar
distributed kernel within floating-point tolerance. The `distributed` command
selects the best backend for the machine automatically, so MPI ranks, OpenMP
threads, and SIMD lanes combine — the MPI + OpenMP + SIMD hybrid at scale.

## Correctness testing

`tests/numerical/simd_equivalence_test.c` runs every implemented SIMD kernel
against the reference on identical inputs and asserts the relative error is
below tolerance, both with and without softening, for all-pairs and
Barnes-Hut. Each test is gated on the CPU actually having the ISA, so a
regression in any backend fails locally on hardware that supports it. The
`distributed_barnes_hut_test` additionally exercises every SIMD distributed
traversal the build compiled in and compares the result to the single-rank
reference.
