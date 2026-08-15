# HPCSim

A native CPU HPC simulation and visualization engine for large-scale gravitational N-body systems.

- C17 numerical kernels + C++20 application layer
- OpenMP shared-memory parallelism with configurable thread counts
- Runtime SIMD dispatch (SSE2 / AVX2 / AVX-512 / ARM NEON) with FMA
- Barnes-Hut O(N log N) octree with Morton-order cache optimization
- SDL3, OpenGL, Dear ImGui visualization
- 10M+ particle target; 100M–1B benchmark infrastructure

Physics is **CPU-only**. The GPU is used strictly for visualization.

## Highlights

- **Two-body validation**: a circular orbit stays an orbit; energy, momentum, and
  center of mass are conserved to the accuracy a symplectic integrator provides.
- **Reference-first development**: a deliberately simple scalar O(N²) kernel is
  the correctness authority; OpenMP, SIMD, and Barnes-Hut are validated against it.
- **Measured, not claimed**: every optimization in this repository was benchmarked
  on the machine it was developed on. Numbers below are real runs, not estimates.

## Measured performance

Developed on an Intel Core Ultra 7 255H (16 threads, AVX2). Force evaluation time,
16384 particles:

| Algorithm | ms / evaluation | vs. reference |
|-----------|-----------------|---------------|
| Reference (scalar, 1 thread) | 749.8 | 1.0x |
| AVX2 SIMD (1 thread) | 163.9 | 4.6x |
| OpenMP + AVX2 (16 threads) | 27.9 | 26.9x |
| Barnes-Hut (theta 0.7, 1 thread) | 48.3 | 15.5x (at 65536 particles: 238x) |

At 65536 particles the all-pairs kernels are quadratic while Barnes-Hut is
O(N log N): reference 11,523 ms vs Barnes-Hut 48 ms per evaluation (238x).

Barnes-Hut scaling at 1,000,000 particles, 16 threads (theta 0.7):

- Tree build: 543 ms
- Force evaluation: 765 ms
- Step: 1.39 s

At 10,000,000 particles: tree build 7.4 s, force evaluation 8.9 s, step 17.5 s.

Reproduce with:

```
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm reference --threads 1
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm avx2 --threads 1
n_body_sim_pro benchmark --particles 16384 --steps 3 --algorithm openmp_avx2 --threads 1,2,4,8,16
n_body_sim_pro benchmark --particles 1000000 --steps 3 --algorithm barnes_hut --threads 16
```

Every run prints machine-readable JSON as its final line, e.g.:

```
{"cpu":"Intel(R) Core(TM) Ultra 7 255H","simd":{"avx2":true,...},"particles":1000000,...}
```

## Building

Requires a C17 and C++20 toolchain, CMake 3.28+, SDL3, and Dear ImGui
(fetched automatically). GCC and Clang are first-class; MSVC is secondary.

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake presets are provided:

```
cmake --preset debug          # assertions + debug info
cmake --preset release        # -O2
cmake --preset relwithdebinfo # for profiling
cmake --preset sanitizer      # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset benchmark      # headless, no GUI, tests off
```

## Usage

```
n_body_sim_pro                    # interactive application
n_body_sim_pro hardware           # detected CPU / SIMD / OpenMP
n_body_sim_pro benchmark --help   # headless benchmarks
n_body_sim_pro save galaxy.hpcs --particles 65536 --preset galaxy_collision
n_body_sim_pro resume galaxy.hpcs --steps 100 --threads 16
```

The interactive application provides:

- nine astrophysical presets (two body, random cloud, solar system, open
  cluster, globular cluster, spiral galaxy, elliptical galaxy, galaxy
  collision, triple galaxy) with deterministic seeds
- reference / OpenMP / SIMD / Barnes-Hut algorithm selection with adjustable
  theta (the theta/accuracy trade-off is measurable in the UI)
- a technical interface: simulation, numerics, performance, memory, and
  developer-console panels backed entirely by real instrumentation
- orbit camera (left-drag rotate, middle-drag pan, scroll zoom)

## Architecture

```
C++ application layer          C17 HPC engine
  Application                    Particle system (SoA, 64-byte aligned)
  SimulationController           N-body physics (reference first)
  Renderer (SDL3/OpenGL)         Integrators (Euler, Leapfrog, Verlet)
  Camera                         Barnes-Hut octree (contiguous nodes)
  UserInterface (Dear ImGui)     Numerical diagnostics
  BenchmarkManager               Allocation layer + tracker
  Instrumentation / Logging      Morton reordering, radix sort
                                SIMD (scalar/SSE2/AVX2/AVX-512/NEON)
                                Threading (OpenMP)
```

The C engine exposes a plain C ABI (`include/hpcsim/hpcsim.h`); the C++
layer wraps it in RAII. The numerical kernels are data-oriented and
procedural; object-oriented structure lives in the application layer.

Why these choices exist is documented in [`docs/`](docs/):

- [`docs/architecture/architecture.md`](docs/architecture/architecture.md)
  — C/C++ boundary, memory model, pipeline, SIMD dispatch, instrumentation
- [`docs/physics/gravity.md`](docs/physics/gravity.md) — equations, softening
- [`docs/physics/integrators.md`](docs/physics/integrators.md) — integrator
  properties
- [`docs/physics/presets.md`](docs/physics/presets.md) — preset assumptions
- [`docs/performance/benchmarks.md`](docs/performance/benchmarks.md) — real
  measured results and how to reproduce them
- [`docs/performance/simd.md`](docs/performance/simd.md) — SIMD coverage

## Roadmap

- [x] Reference O(N²), two-body validation
- [x] OpenMP parallel kernels and thread control
- [x] Runtime SIMD dispatch with AVX2 kernel
- [x] Barnes-Hut octree with theta control and Morton-order optimization
- [x] Structured logging, allocation tracking, phase telemetry, developer UI
- [x] Headless benchmark CLI with JSON output, memory estimation, checkpoints
- [ ] SIMD Barnes-Hut force traversal
- [ ] NUMA-aware placement
- [ ] MPI + OpenMP + SIMD distributed execution

## License

MIT — see [LICENSE](LICENSE).
