# SIMD

## Kernel coverage matrix

A SIMD backend is only *selected* when a real kernel exists for it. The
detected ISA and the selected backend are distinct facts and are reported
separately in the UI and `hardware` command. Each backend below has a real
all-pairs kernel and a Barnes-Hut kernel; a backend is selected only when the
CPU has the ISA *and* the matching translation unit was compiled with the
ISA's flags (CMake sets `N_BODY_SIM_PRO_HAVE_*_KERNEL` accordingly).

| Backend | Detection | Kernel status |
|---------|-----------|---------------|
| Scalar | always | reference kernel (correctness authority) |
| SSE2 | CPUID | detected, but no SSE2-specific kernel; SSE2 stays scalar |
| AVX2 + FMA | CPUID | all-pairs and Barnes-Hut kernels, 256-bit lanes |
| AVX-512 | CPUID | all-pairs and Barnes-Hut kernels, 512-bit lanes |
| NEON | ARM64 (always present) | all-pairs and Barnes-Hut kernels, 128-bit lanes |

The selected backend is reported honestly: on this project's development
machine it is `AVX2` (the CPU has no AVX-512). On a machine with AVX-512 the
selected backend is `AVX-512`; on an ARM64 machine it is `NEON`.

## All-pairs force kernels

All SIMD all-pairs kernels vectorize the inner loop over source particles
(4 at a time for AVX2, 8 for AVX-512, 2 for NEON): the coordinate deltas, the
distance, and the softened inverse-distance-cubed are computed in vector
lanes, and the acceleration accumulation uses fused multiply-add. Each is
compiled in its own translation unit with the matching flags, so the rest of
the engine does not require the ISA.

Two correctness details apply to every kernel:

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
below tolerance, both with and without softening, for all-pairs and
Barnes-Hut. Each test is gated on the CPU actually having the ISA, so a
regression in any backend fails CI-style runs locally on hardware that
supports it.

## SIMD Barnes-Hut

SIMD Barnes-Hut force kernels are implemented for AVX2, AVX-512, and NEON
(`barnes_hut_avx2.c`, `barnes_hut_avx512.c`, `barnes_hut_neon.c`) and
validated against the scalar kernel to 1e-9 relative (identical acceptance
decisions; only the floating-point summation order differs). The traversal
mirrors the scalar walk exactly and stages the accepted interactions into
SIMD slots (4, 8, or 2), flushing them into register accumulators with FMA.

Measured on the development machine (Intel Core Ultra 7 255H, AVX2), the AVX2
SIMD kernel is **not** faster than the scalar one:

| N (1 thread) | scalar Barnes-Hut | SIMD Barnes-Hut (AVX2) |
|--------------|-------------------|------------------------|
| 65,536 | 68 ms | 300 ms |
| 262,144 | 234 ms | 1,643 ms |

The traversal is memory-latency bound: each particle walks ~30-60 tree nodes
with random access, and vectorizing the per-interaction arithmetic adds
staging overhead without removing the cache misses. The scalar kernel is the
default; the SIMD variants are available for experimentation and for hardware
where the arithmetic share of the cost is larger. These numbers are reported
rather than hidden: an optimization that does not measure faster is not
claimed as one.

## SIMD distributed traversal

The distributed Barnes-Hut traversal is also SIMD-accelerated
(`distributed_barnes_hut_avx2.c`, `_avx512.c`, `_neon.c`). The essential-tree
exchange and the local tree build are byte-identical to the scalar kernel;
only the per-particle force accumulation over the local tree and the remote
essential forest is staged and applied with vector FMA. The opening decisions
are identical, so the result matches the scalar distributed kernel within
floating-point tolerance. The `distributed` command selects the best backend
for the machine automatically, giving the MPI + OpenMP + SIMD hybrid at
scale.
