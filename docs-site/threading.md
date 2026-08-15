# OpenMP & Threading

## Design

OpenMP parallelizes the **outer particle loop** of the force kernels with a
static schedule. Two properties make this sound:

1. **Bit-identical all-pairs**: each particle's inner force sum is unchanged,
   so the OpenMP all-pairs kernel reproduces the reference bit-for-bit on a
   given machine. Parallelism with zero numerical change.
2. **Read-only tree**: the Barnes-Hut traversal is parallelized over query
   particles while the tree itself is only read, so no locking is needed.

## Thread configuration

`hpcsim_threading_set_thread_count(n)` configures the active thread count;
`n <= 0` restores the default (all available threads). Nothing artificially
limits the count. The UI offers Auto/1/2/4/8/16/32/64.

The threading module is the single place that configures OpenMP. When the
engine is built without OpenMP, every function degrades to a single-threaded
default and `hpcsim_threading_openmp_available()` returns 0.

## Measured scaling

16,384 particles, OpenMP + AVX2:

| threads | ms / evaluation | speedup | efficiency |
|---------|-----------------|---------|------------|
| 1 | 178.5 | 1.00 | — |
| 2 | 98.6 | 1.81 | 0.90 |
| 4 | 50.7 | 3.52 | 0.88 |
| 8 | 36.6 | 4.87 | 0.61 |
| 16 | 27.9 | 6.41 | 0.40 |

Efficiency is ~90% at 2–4 threads and degrades beyond that because the
kernel is **memory-bandwidth bound**: all threads stream the entire position
and mass arrays for every particle. This is the expected behavior for an
all-pairs kernel, and it is reported honestly rather than hidden.

## Thread affinity

The engine can pin threads to logical processors with a policy:

| Policy | Behavior |
|--------|----------|
| AUTO | no pinning; let the OS decide |
| COMPACT | fill logical processors in order |
| SPREAD | one thread per NUMA node before reuse |

Affinity is best-effort: pinning can fail in restricted environments and the
call simply returns non-zero. It is implemented with `SetThreadAffinityMask`
on Windows and `sched_setaffinity` on Linux, isolated behind
`hpcsim_thread_affinity_apply_policy`.

## Hot-loop discipline

The inner force loop is kept free of virtual dispatch, allocation, string
operations, logging, and locks. The OpenMP reduction of per-particle
statistics uses `reduction(+:...)` clauses so threads never contend on a
shared counter. No logging calls exist in the numerical loops; the
structured logger is used exclusively at application level.
