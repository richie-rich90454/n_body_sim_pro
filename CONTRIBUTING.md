# Contributing

## Ground rules

1. **Correctness before optimization.** The scalar reference kernel is the
   authority; every optimized kernel must pass the equivalence tests.
2. **Readability is a feature.** Prefer long descriptive names over short
   ones. Do not micro-optimize at the cost of comprehension.
3. **Optimizations must be measured.** Add a baseline benchmark, change the
   code, rerun, record the result. Keep or revert based on the data. Never
   claim speedups without a reproducible benchmark.
4. **C is C, C++ is C++.** C17 kernels stay idiomatic C (opaque types,
   explicit error codes, `restrict` where justified). C++20 stays idiomatic
   modern C++ (RAII, no C-with-classes).
5. **No fake anything.** No fabricated benchmark numbers, CPU utilization,
   memory statistics, or SIMD counters. If something is unavailable, report
   `N/A`.

## Git history

- **One file per commit.** A commit modifies exactly one file wherever
  possible. Split implementation, tests, and CMake changes into separate
  commits.
- **Commit frequently.** Each logical unit is its own commit. Do not
  accumulate large uncommitted batches, and never make a giant
  "implementation complete" commit.
- **Do not commit generated files.** `build/`, `cmake-build-*/`, runtime
  logs, and benchmark dumps are gitignored.

## Before you commit

1. `git status` and `git diff` — confirm exactly one intended file changed.
2. Build with the `debug` preset; it enables the strict warning set. Aim for
   zero warnings.
3. Run `ctest --test-dir build/debug` — all tests must pass.
4. Run the `sanitizer` preset for memory-sensitive changes
   (`cmake --preset sanitizer && ctest --test-dir build/sanitizer`).
5. Benchmark performance-sensitive changes and record the numbers in
   `docs/performance/benchmarks.md`.

## Code layout

- `include/hpcsim/` — public C API headers.
- `src/c/` — C17 engine (core, physics, barnes_hut, memory, simd, threading,
  generation, diagnostics, checkpoint).
- `src/cpp/` — C++20 application layer (application, simulation, rendering,
  ui, logging, instrumentation, benchmark).
- `tests/` — unit, numerical, and regression tests, each its own executable.
- `benchmarks/` — headless benchmark executables.
- `docs/` — architecture and physics rationale, real benchmark results.
