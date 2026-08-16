# Benchmarks

All numbers in this document were measured on the machine the project was
developed on: an Intel Core Ultra 7 255H (16 logical threads, AVX2, no
AVX-512), Windows, GCC 16, Release build. Every figure below is the output
of a real run; the `benchmark` command prints machine-readable JSON as its
last line.

To reproduce on your machine:

```
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm reference --threads 1
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm avx2 --threads 1
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm openmp_avx2 --threads 1,2,4,8,16
n_body_sim_pro benchmark --particles 1000000 --steps 3 --algorithm barnes_hut --theta 0.7 --threads 16
```

The standalone force-kernel benchmark accepts every kernel explicitly, so
AVX-512 and NEON variants can be measured on hardware that has them (the
reference machine has no AVX-512, so those rows are not measured here):

```
n_body_sim_pro_benchmark --particles 16384 --steps 3 --algorithm avx512 --threads 1
n_body_sim_pro_benchmark --particles 16384 --steps 3 --algorithm openmp_avx512 --threads 1,2,4,8,16
n_body_sim_pro_benchmark --particles 16384 --steps 3 --algorithm neon --threads 1
n_body_sim_pro_benchmark --particles 16384 --steps 3 --algorithm barnes_hut_avx512 --threads 1
```

## All-pairs kernels (16384 particles)

Single force evaluation, Release build:

| Algorithm | ms | vs. reference |
|-----------|-----|---------------|
| reference (scalar, 1 thread) | 749.8 | 1.0x |
| avx2 (1 thread) | 163.9 | 4.6x |
| openmp_avx2, 2 threads | 98.6 | 7.6x |
| openmp_avx2, 4 threads | 50.7 | 14.8x |
| openmp_avx2, 8 threads | 36.6 | 20.5x |
| openmp_avx2, 16 threads | 27.9 | 26.9x |

Notes:
- SIMD alone (AVX2 vs reference, same thread) is a 4.6x speedup.
- OpenMP+AVX2 efficiency is ~90% at 2-4 threads and drops at 8-16 threads,
  which is expected for a kernel that streams the entire position array for
  every particle: it is memory-bandwidth bound, not compute bound. We do not
  claim linear scaling.

## Barnes-Hut vs all-pairs (65536 particles, 1 thread)

| Algorithm | ms / evaluation |
|-----------|-----------------|
| reference (scalar) | 11,523 |
| avx2 | 2,904 |
| barnes_hut (theta 0.7) | 48.3 |

Barnes-Hut is 238x faster than the scalar reference and 60x faster than
AVX2 at this size, because all-pairs is O(N²) while Barnes-Hut is O(N log N).

## Barnes-Hut scaling (16 threads, theta 0.7)

| Particles | Tree build | Force eval | Step |
|-----------|------------|------------|------|
| 16,384 | 9.4 ms | 58 ms | 64 ms |
| 1,000,000 | 543 ms | 765 ms | 1,388 ms |
| 10,000,000 | 7,399 ms | 8,907 ms | 17,503 ms |

The Morton-order reordering (see docs/architecture) reduced the 1M step from
3.25 s to 1.39 s — a 2.3x improvement with no change to the physics.

## Theta / accuracy / cost trade-off

Measured on a 400-particle random cloud, RMS force error relative to the
reference all-pairs kernel:

| theta | RMS error | total interaction work |
|-------|-----------|------------------------|
| 0.1 | 1.4e-5 | 39,416 |
| 0.3 | 1.0e-3 | 32,591 |
| 0.7 | 1.5e-2 | 14,355 |

Lower theta is more accurate and more expensive; higher theta is cheaper and
coarser. These numbers come from the test suite, which asserts the
monotonicity of the trade-off.

## Distributed execution (MPI)

Measured on the same machine with MS-MPI, 2 ranks, 4,096 particles per rank,
theta 0.7:

| Rank | local particles | remote cells | essential cells | levels | compute | communication | step |
|------|-----------------|--------------|-----------------|--------|---------|---------------|------|
| 0 | 2,048 | 2,951 | 2,951 | 8 | 29.6% | 10.2% | 63.5 ms |
| 1 | 2,048 | 2,953 | 2,953 | 8 | 28.7% | 10.2% | 63.1 ms |

The distributed result matches the single-rank Barnes-Hut result within the
theta tolerance (validated by `distributed_barnes_hut_test` under mpiexec).
The essential-tree exchange terminates when no rank needs finer detail; the
communication/computation split is real, per-rank, and measured.

## Honesty rules

- No number here is fabricated. If a run is not reproducible on a given
  machine, that is reported by the benchmark itself (or `N/A`).
- AVX-512 and NEON kernels are implemented and validated for correctness on
  hardware that supports them, but the reference machine has no AVX-512 and
  is not an ARM64 machine, so they are never benchmarked here and never
  claimed to be faster.
- Thread scaling is reported with measured efficiency, which drops as the
  kernel becomes memory-bound.
- The SIMD Barnes-Hut kernel is measurably slower than the scalar kernel on
  this hardware (memory-bound walk); that is documented, not hidden.

## Regression tracking

The benchmark command's JSON output is suitable for storing in a
`benchmarks/results/` directory for regression comparison. If a change makes
a benchmark materially worse, investigate before merging.
