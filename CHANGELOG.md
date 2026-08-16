# Changelog

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); the project is pre-1.0, so
any part of the API may change.

## [Unreleased]

### Added

- AVX-512 SIMD all-pairs and Barnes-Hut force kernels (512-bit lanes, FMA),
  compiled in their own translation units and selected at runtime when the
  CPU has AVX-512 and the build compiled the kernels.
- NEON SIMD all-pairs and Barnes-Hut force kernels (AArch64, 128-bit lanes,
  FMA), selected at runtime on ARM64.
- SIMD acceleration of the distributed Barnes-Hut traversal for AVX2,
  AVX-512, and NEON: the essential-tree exchange and local tree build are
  byte-identical to the scalar kernel, and only the per-particle force
  accumulation is staged and applied with vector FMA.
- The `distributed` command selects the best SIMD backend for the machine
  automatically and reports it in the per-rank telemetry line, giving the
  MPI + OpenMP + SIMD hybrid at scale.
- Force-kernel benchmark algorithm names for the new kernels (`avx512`,
  `openmp_avx512`, `neon`, `openmp_neon`, `barnes_hut_avx512`,
  `barnes_hut_openmp_avx512`, `barnes_hut_neon`, `barnes_hut_openmp_neon`).

### Changed

- Backend selection prefers AVX-512 over AVX2 over NEON over scalar, and is
  gated on the kernel actually being compiled in (`N_BODY_SIM_PRO_HAVE_*_KERNEL`)
  so a backend is never selected on a build that lacks its kernel.
- The `distributed_barnes_hut_test` now exercises every SIMD distributed
  traversal the build compiled in and checks it against the single-rank
  reference.

### Fixed

- CMake fetches the public Dear ImGui dependency over HTTPS instead of SSH,
  so a build needs no SSH private key.
- CMake resolves the static GLEW target (`GLEW::glew_s`): a system package is
  preferred, and static GLEW is built from source when the platform package
  ships only the shared library (e.g. Homebrew).
- NUMA thread-affinity pinning is guarded to Linux so macOS builds compile.
- CI builds Windows x64, Linux x64, and macOS ARM64, packages each platform,
  and publishes tagged (`vX.Y.Z`) releases; the release secret is only used
  by the release step.

## [0.1.0] - 2026-08-15

### Added

- Repository foundation: C17/C++20 CMake build, CMake presets (debug,
  release, relwithdebinfo, sanitizer, benchmark, developer), aggressive
  per-target compiler warnings, MIT license.
- C17 engine: status/error codes, controlled aligned allocation layer with
  metadata tracking, SoA particle system (10 double arrays, 64-byte aligned),
  reference scalar O(N^2) gravity kernel, Euler/Leapfrog/Velocity-Verlet
  integrators with a force-function callback, conservation diagnostics.
- Initial-condition generation: xoshiro256** deterministic PRNG, nine
  astrophysical presets (Plummer spheres, exponential disks, spiral arms,
  galaxy collision, triple galaxy) with zero-net-momentum enforcement.
- Parallelism: OpenMP-parallel force kernels (bit-identical to the reference
  for all-pairs), runtime thread-count control, thread-scaling benchmark.
- SIMD: runtime CPU feature detection (cpuid/ARM64), backend selection, AVX2
  force kernel with FMA and self-interaction guard, SIMD/reference
  equivalence tests.
- Barnes-Hut: contiguous octree with integer child indices, center-of-mass
  pass, theta-controlled approximation, OpenMP traversal, tree statistics
  (node/leaf counts, depth, approximations, exact interactions, build/eval
  timing), Morton-order particle reordering with LSD radix sort.
- Instrumentation: structured ring-buffer logger with levels/categories and
  a developer console; thread-local-buffered allocation tracker with memory
  and category panels; per-phase telemetry; conservation diagnostics
  (momentum error, center-of-mass offset, energy drift) in the UI.
- Application: SDL3 window, OpenGL 3.3 core context, Dear ImGui technical
  interface, orbit camera, point/line-strip renderer, two-body trails.
- CLI: `hardware`, `benchmark` (headless, with JSON output and memory
  estimation), `save`, `resume`.
- Checkpointing: versioned binary checkpoint format, controller
  save/load, headless resume.
- Testing: unit, numerical, and regression suites (11 executables) including
  the two-body orbital validation and SIMD/Barnes-Hut equivalence tests.
- Documentation: architecture, physics, presets, benchmarks (real measured
  numbers), SIMD coverage, README.

### Performance (measured on Intel Core Ultra 7 255H, see docs/performance)

- 16384 particles: AVX2 4.6x over scalar; OpenMP+AVX2 26.9x at 16 threads.
- 65536 particles: Barnes-Hut 238x over the scalar reference.
- 1M particles, 16 threads: 1.39 s/step; Morton reordering improved steps by
  2.3x.
- 10M particles, 16 threads: 17.5 s/step.
