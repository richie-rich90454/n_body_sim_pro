# N-Body Sim Pro

A native CPU HPC simulation and visualization engine for large-scale gravitational N-body systems.

- C17 numerical kernels + C++20 application layer
- OpenMP shared-memory parallelism with configurable thread counts
- Runtime SIMD dispatch (SSE2 / AVX2 / AVX-512 / ARM NEON) with FMA
- Barnes-Hut O(N log N) octree with Morton-order cache optimization
- SDL3, OpenGL, Dear ImGui visualization
- 10M+ particle target; 100M鈥?B benchmark infrastructure

Physics is **CPU-only**. The GPU is used strictly for visualization.

## Highlights

- **Two-body validation**: a circular orbit stays an orbit; energy, momentum, and
  center of mass are conserved to the accuracy a symplectic integrator provides.
- **Reference-first development**: a deliberately simple scalar O(N虏) kernel is
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

## Prerequisites

- A **C17 + C++20** toolchain. GCC and Clang are first-class; MSVC is
  secondary support (not the design target).
- **CMake** 3.28 or newer and a generator (Ninja recommended).
- **SDL3** development libraries. On MSYS2/ucrt64:
  `pacman -S mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-glew`. On
  Debian/Ubuntu: `libsdl3-dev`, `libglew-dev`. On macOS:
  `brew install sdl3 glew`.
- **OpenMP** support in the compiler (for threaded kernels).
- **Microsoft MPI** (Windows) or an MPI implementation such as OpenMPI or
  MPICH (Linux/macOS) 鈥?only needed for the distributed (`mpiexec`) path;
  everything else builds and runs without it.
- **Dear ImGui** is fetched automatically by CMake over Git (SSH) 鈥?no
  manual step.

## Building

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake presets cover the common configurations:

```
cmake --preset debug            # assertions + debug info
cmake --preset release          # optimized
cmake --preset relwithdebinfo   # optimized + debug info (for profiling)
cmake --preset sanitizer        # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset benchmark        # headless, no GUI, tests off
```

The presets point at the MSYS2 ucrt64 toolchain and prefix path; adjust
`CMakePresets.json` if your environment differs. MPI is detected
automatically (`-DN_BODY_SIM_PRO_ENABLE_MPI=ON` by default); pass
`-DN_BODY_SIM_PRO_ENABLE_MPI=OFF` to build without it.

## Usage

```
n_body_sim_pro                          # launch the interactive application
n_body_sim_pro hardware                 # detected CPU / SIMD / OpenMP / NUMA
n_body_sim_pro benchmark --help         # headless benchmarks (JSON output)
n_body_sim_pro save galaxy.hpcs --particles 65536 --preset galaxy_collision
n_body_sim_pro resume galaxy.hpcs --steps 100 --threads 16
mpiexec -n 2 n_body_sim_pro distributed --particles 4096 --steps 5
```

### Launching the interactive application

Running `n_body_sim_pro` with no arguments opens the SDL3 window. The
default scene is a two-body orbit with trails. Use the **Simulation** panel
to pick a preset, particle count, seed, and algorithm:

- **All-pairs (OpenMP)** 鈥?exact O(N虏), parallel.
- **All-pairs (single thread)** 鈥?the scalar reference.
- **Barnes-Hut** 鈥?O(N log N), with a 胃 slider.
- **Barnes-Hut (SIMD, experimental)** 鈥?the SIMD traversal (benchmarked
  slower than scalar on most hardware; documented).

Controls:

- Left-drag rotates the camera, middle-drag pans, scroll zooms.
- **Play/Pause/Step** drive the simulation; **Steps/frame** controls speed.
- The **Numerics** panel shows real momentum error and energy drift;
  **Performance** shows measured step/tree/force timings; **Memory** shows
  live allocations by category; the **Developer Console** streams structured
  log records.

The application is a scientific workstation UI, not a consumer dashboard:
every number shown is measured, and unavailable metrics render as `N/A`.

### Headless and distributed

`benchmark` runs without any window and prints human-readable output plus a
machine-readable JSON line. `distributed` requires MPI:

```
mpiexec -n 2 build/release/bin/n_body_sim_pro distributed --particles 4096 --steps 5
```

Each rank reports its own particle block, remote essential-tree cells,
exchange levels, and the communication/computation split.

## Architecture

```
C++ application layer          C17 HPC engine
  Application                    Particle system (SoA, 64-byte aligned)
  SimulationController           N-body physics (reference first)
  Renderer (SDL3/OpenGL)         Integrators (Euler, Leapfrog, Verlet)
  Camera                         Barnes-Hut octree (contiguous nodes)
  UserInterface (Dear ImGui)     Numerical diagnostics
  BenchmarkManager               Allocation layer + tracker
  Logging                        Morton reordering, radix sort
  Distributed runner             SIMD (scalar/SSE2/AVX2/AVX-512/NEON)
                                 Threading + NUMA (OpenMP)
                                 MPI essential-tree exchange
```

The C engine exposes a plain C ABI (`include/n_body_sim_pro/n_body_sim_pro.h`); the C++
layer wraps it in RAII. Physics is CPU-only; OpenGL is strictly
visualization.

## Documentation

Full technical documentation (architecture, physics, performance with
measured results, SIMD, NUMA, MPI, CLI reference) is published as a
[VitePress site](https://github.com/anomalyco/opencode) built from
[`docs-site/`](docs-site/). The in-repo rationale lives in
[`docs/`](docs/).

- [`docs/architecture/architecture.md`](docs/architecture/architecture.md)
  鈥?C/C++ boundary, memory model, pipeline, SIMD dispatch, instrumentation
- [`docs/physics/gravity.md`](docs/physics/gravity.md) 鈥?equations, softening
- [`docs/physics/integrators.md`](docs/physics/integrators.md) 鈥?integrator
  properties
- [`docs/physics/presets.md`](docs/physics/presets.md) 鈥?preset assumptions
- [`docs/performance/benchmarks.md`](docs/performance/benchmarks.md) 鈥?real
  measured results and how to reproduce them
- [`docs/performance/simd.md`](docs/performance/simd.md) 鈥?SIMD coverage

## Roadmap

- [x] Reference O(N虏), two-body validation
- [x] OpenMP parallel kernels and thread control
- [x] Runtime SIMD dispatch with AVX2 kernel
- [x] Barnes-Hut octree with theta control and Morton-order optimization
- [x] SIMD Barnes-Hut force traversal (implemented; benchmarked slower than
      scalar on the reference hardware, documented honestly)
- [x] Structured logging, allocation tracking, phase telemetry, developer UI
- [x] Headless benchmark CLI with JSON output, memory estimation, checkpoints
- [x] NUMA topology detection, thread affinity policies, first-touch
      parallel generation
- [x] MPI distributed execution with a local essential-tree exchange and
      per-rank telemetry
- [ ] SIMD acceleration of the distributed traversal
- [ ] AVX-512 / NEON all-pairs and Barnes-Hut kernels
- [ ] MPI + OpenMP + SIMD hybrid at scale

## License

MIT 鈥?see [LICENSE](LICENSE).
