# Architecture

This document explains the *why* of the major architectural decisions, not
just the *what*. Every decision here exists to serve a specific, stated goal.

## C / C++ boundary

The engine is two layers joined by a plain C ABI.

- **C17 engine** (`src/c`, public headers in `include/hpcsim/`): numerical
  kernels, particle storage, Barnes-Hut, diagnostics, allocation, SIMD
  backends, threading. Data-oriented, procedural, opaque types, explicit
  error codes. No global mutable state, no C++-isms.
- **C++20 layer** (`src/cpp`): application, simulation controller, renderer,
  camera, UI, logging, benchmarking. Uses RAII, `std::unique_ptr`, `std::span`
  where appropriate, `enum class`, exceptions at layer boundaries.

**Why**: the numerical kernels are validated and benchmarked against a scalar
reference; a plain C ABI keeps them decoupled from any application concerns
and testable in isolation. The C++ layer can then be as expressive as
desired without leaking into hot loops.

## Particle storage: Structure of Arrays

Particles are stored as ten separate contiguous `double` arrays
(positions x/y/z, velocities x/y/z, accelerations x/y/z, masses), each
64-byte aligned.

**Why**:
- SIMD kernels operate on one physical quantity at a time (all x positions,
  then all y positions...). SoA keeps each stream contiguous, so vector
  loads/stores stay dense and cache lines are fully used.
- Different quantities change at different times during a timestep (forces
  accumulate into acceleration; integration reads position+velocity).
  Separating them avoids writing entire large particle structs when only one
  quantity changes.

## Reference first

`hpcsim_gravity_compute_acceleration_reference` is the deliberate O(N²)
single-threaded scalar correctness authority. It is never deleted and never
"fixed up" to be faster.

**Why**: every optimized kernel (OpenMP, SIMD, Barnes-Hut) is validated
against it. A wrong-but-fast kernel is useless; a readable slow reference
makes correctness testing straightforward and gives every optimization a
ground truth to be measured against.

## Barnes-Hut octree

The tree is a contiguous node array with integer child indices; no node is
individually heap-allocated. Leaf capacity is one particle. Children always
have higher indices than their parents, so a reverse-index pass is a valid
post-order traversal for center-of-mass accumulation.

**Why**: pointer-heavy octrees fragment memory and destroy cache behavior at
10M particles. Integer indices keep the tree compact, relocatable, and
addressable with minimal metadata.

Particles are reordered by a 63-bit Morton (Z-order) key computed from
quantized positions, sorted with a 3-pass LSD radix sort, and the tree is
built and traversed in that order.

**Why**: spatially near particles share tree paths. Sorting by Z-order makes
both the build and the force traversal touch nodes sequentially instead of
at random, converting L3-cache misses into hits. Measured effect at 1M
particles, 16 threads: step time dropped from 3.25 s to 1.39 s.

## Runtime SIMD dispatch

At startup the engine detects CPU features (`cpuid` on x86, compile-time
architecture on ARM64) and selects the best backend for which a real kernel
exists. The selected backend is what is actually used and what the UI
reports.

**Why**: a binary compiled for one ISA either crashes or silently falls back
on other hardware. Dispatch keeps one binary correct everywhere and honest
about what it runs.

A backend is only selected when a real implementation exists. AVX-512 and
NEON kernels are declared but not yet implemented; they are never selected
until they exist, and the UI reports "unavailable" rather than pretending.

## OpenMP

The force kernels parallelize the outer particle loop with a static
schedule. The reference OpenMP kernel keeps each particle's inner sum in the
same order, so it is bit-identical to the reference on a given machine —
parallelism with zero numerical change. The Barnes-Hut traversal is
parallelized the same way over query particles; the tree itself is
read-only during evaluation, so no locking is needed.

Thread count is configurable at runtime and defaults to all available
threads. Nothing artificially limits it to 4 or 8.

## Allocation and tracking

Internal allocations go through `hpcsim_allocate`/`hpcsim_deallocate`, which
attach a header carrying size, alignment, and category. Third-party
libraries never use this layer and the process allocator is never replaced.

The allocation tracker buffers events in thread-local staging and flushes
them in batches under one lock, so the hot allocation path is a TLS store
plus one flag check. When disabled (the default in release builds) it is a
single branch. This is deliberate: per-allocation `printf` would destroy
benchmark validity.

## Instrumentation

- Structured logger with severity, category, thread id, and a bounded ring
  buffer rendered in the developer console. Never called from hot loops.
- Phase telemetry: the Barnes-Hut kernel measures tree build and evaluation
  time internally; the controller measures full step time. All values shown
  in the UI come from these real timers.
- Conservation diagnostics (momentum error, center-of-mass offset, energy
  drift) are computed from the actual particle state each step. Energy drift
  requires the O(N²) potential sum, so it is only tracked for systems small
  enough to afford it; for larger systems the UI reports `N/A` rather than a
  fabricated number.

## Renderer

Physics never depends on OpenGL. The controller owns the particle system;
the renderer uploads a snapshot of positions each frame and draws them as
`GL_POINTS` plus line strips. Rendering never mutates simulation state.
