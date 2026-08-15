# Getting Started

This guide walks through prerequisites, building every configuration, and
launching both the interactive application and the headless tools. It is
specific about commands and expected output; nothing here is assumed.

## Prerequisites

### Toolchain

A **C17 + C++20** toolchain with **OpenMP**. GCC and Clang are first-class;
MSVC is secondary support.

| Component | Minimum | Notes |
|-----------|---------|-------|
| CMake | 3.28 | presets require it |
| Ninja | any | recommended generator |
| C compiler | C17 | GCC 鈮?13 or Clang 鈮?16 recommended |
| C++ compiler | C++20 | matching the C compiler |
| OpenMP | supported | threaded kernels; degrades to serial if absent |

### Libraries

**SDL3** and **GLEW** (for the OpenGL loader) are required for the
interactive application. **Dear ImGui** is fetched automatically by CMake
over Git (SSH). **MS-MPI / OpenMPI / MPICH** is only required for the
distributed `mpiexec` path.

On MSYS2 / ucrt64 (Windows):

```bash
pacman -S mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-glew
pacman -S mingw-w64-ucrt-x86_64-msmpi      # optional, for MPI
```

On Debian / Ubuntu:

```bash
sudo apt install libsdl3-dev libglew-dev libopenmpi-dev  # openmpi optional
```

On macOS (Homebrew):

```bash
brew install sdl3 glew open-mpi   # open-mpi optional
```

::: tip
The MSYS2 ucrt64 package names differ from mingw64. Use the `ucrt64`
variants when building with the `ucrt64` GCC toolchain, which is what the
CMake presets target.
:::

### MPI runtime (Windows)

The MSYS2 `msmpi` package ships the SDK (headers + import library) but not
the runtime `msmpi.dll`/`mpiexec`. The Microsoft MPI redistributable must
be installed, or the runtime extracted into a local directory that is added
to `PATH`:

```powershell
# After downloading MSMpiSetup.exe from Microsoft, or extracting it:
#   - msmpi.dll (x64) next to the binaries that link it
#   - mpiexec.exe, smpd.exe in a directory on PATH
```

## Building

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

The first configure fetches Dear ImGui over SSH, which can take a few
minutes. Subsequent configures are fast.

### CMake presets

| Preset | Build type | Purpose |
|--------|------------|---------|
| `debug` | Debug | assertions, `-O0`, full instrumentation |
| `release` | Release | optimized, used for benchmarks |
| `relwithdebinfo` | RelWithDebInfo | optimized + symbols, for profiling |
| `sanitizer` | Debug + ASan/UBSan | memory-safety verification |
| `benchmark` | Release, no GUI | headless benchmarking |
| `developer` | RelWithDebInfo + developer mode | instrumentation |

```bash
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release
```

The presets in `CMakePresets.json` point at the MSYS2 ucrt64 toolchain and
prefix path. Adjust the `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, and
`CMAKE_PREFIX_PATH` entries if your environment differs.

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `N_BODY_SIM_PRO_ENABLE_OPENMP` | ON | link OpenMP and compile parallel kernels |
| `N_BODY_SIM_PRO_ENABLE_MPI` | ON | detect and link MPI when available |
| `N_BODY_SIM_PRO_ENABLE_DEVELOPER_MODE` | ON | allocation tracking + developer UI |
| `N_BODY_SIM_PRO_BUILD_TESTS` | ON | build the CTest suite |
| `N_BODY_SIM_PRO_BUILD_BENCHMARKS` | ON | build the benchmark executable |
| `N_BODY_SIM_PRO_BUILD_APPLICATION` | ON | build the SDL3 application |
| `N_BODY_SIM_PRO_ENABLE_SANITIZERS` | OFF | compile with ASan + UBSan |
| `N_BODY_SIM_PRO_ENABLE_LTO` | OFF | interprocedural optimization in Release |

```bash
cmake -S . -B build -DN_BODY_SIM_PRO_ENABLE_MPI=OFF   # skip MPI entirely
```

### Warning and sanitizer hygiene

Every N-Body Sim Pro target is compiled with an aggressive warning set (`-Wall
-Wextra -Wpedantic -Wconversion -Wshadow ...`) applied per-target, so
third-party libraries are not drowned in them. The build aims for zero
warnings. The `sanitizer` preset links AddressSanitizer and
UndefinedBehaviorSanitizer; on MSYS2 the ucrt64 GCC does not ship `libasan`,
so use the clang64 toolchain for sanitizer runs:

```bash
cmake -S . -B build/asan \
  -DCMAKE_C_COMPILER=C:/msys64/clang64/bin/clang.exe \
  -DCMAKE_CXX_COMPILER=C:/msys64/clang64/bin/clang++.exe \
  -DCMAKE_PREFIX_PATH=C:/msys64/clang64 \
  -DN_BODY_SIM_PRO_ENABLE_SANITIZERS=ON -DN_BODY_SIM_PRO_BUILD_APPLICATION=OFF
cmake --build build/asan
ctest --test-dir build/asan
```

## Running the tests

All tests are plain executables registered with CTest; the numerical engine
has no GUI dependency.

```bash
ctest --test-dir build/release --output-on-failure
```

The suite covers allocator and particle-system units, gravity and
diagnostics numerics, preset determinism, the two-body regression, checkpoint
round-trips, OpenMP/SIMD/Barnes-Hut equivalence, NUMA behavior, and 鈥?when
MPI is present 鈥?a 2-rank distributed equivalence test launched through
`mpiexec`.

## Launching the interactive application

```bash
./build/release/bin/n_body_sim_pro
```

Expect a window titled **N-Body Sim Pro 鈥?CPU N-Body Simulation Engine** on a
near-black background. The default scene is a **two-body orbit** with motion
trails. Camera controls:

| Input | Action |
|-------|--------|
| Left-drag | orbit |
| Middle-drag | pan |
| Scroll | zoom |

Use the **Simulation** panel on the left to select a preset, particle count,
seed, algorithm, integrator, and timestep, then press **Play**. The
**Numerics** panel shows real conservation errors; **Performance** shows
measured timings; **Memory** shows live allocations by category; the
**Developer Console** at the bottom streams structured log records.

::: warning
With **All-pairs** selected, particle counts above ~64k run slowly by design
(O(N虏)). Select **Barnes-Hut** for large systems.
:::

## Running headless

```bash
# Hardware report (CPU / SIMD / OpenMP / NUMA)
./build/release/bin/n_body_sim_pro hardware

# Force-kernel benchmark, machine-readable JSON as the last line
./build/release/bin/n_body_sim_pro benchmark --particles 16384 --steps 3 \
  --algorithm openmp_avx2 --threads 1,2,4,8,16

# Save a preset as a checkpoint, then resume it
./build/release/bin/n_body_sim_pro save galaxy.hpcs --particles 65536 --preset galaxy_collision
./build/release/bin/n_body_sim_pro resume galaxy.hpcs --steps 100 --threads 16

# Distributed (requires MPI and mpiexec)
mpiexec -n 2 ./build/release/bin/n_body_sim_pro distributed --particles 4096 --steps 5
```

See [Command Line Reference](/cli) for the full flag set.

## Where things live

| Path | Contents |
|------|----------|
| `include/n_body_sim_pro/` | public C API headers |
| `src/c/` | C17 engine (core, physics, barnes_hut, memory, simd, threading, mpi, ...) |
| `src/cpp/` | C++20 application layer |
| `tests/` | unit, numerical, regression, integration suites |
| `benchmarks/` | headless benchmark executables |
| `docs/` | in-repo architecture and physics rationale |
| `docs-site/` | this VitePress site |
