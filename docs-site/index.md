# N-Body Sim Pro

> A native **CPU** HPC simulation and visualization engine for large-scale
> gravitational N-body systems.

```mermaid
flowchart LR
    subgraph C["C17 numerical engine"]
        P["SoA particle system"]
        G["Reference O(N虏) gravity"]
        I["Euler 路 Leapfrog 路 Verlet"]
        B["Barnes-Hut octree"]
        D["Diagnostics"]
    end
    subgraph CXX["C++20 application layer"]
        SC["SimulationController"]
        R["Renderer (SDL3 + OpenGL)"]
        UI["Dear ImGui panels"]
        INS["Instrumentation"]
    end
    subgraph PERF["Performance layer"]
        OMP["OpenMP"]
        SIMD["AVX2 路 SSE2 路 AVX-512 路 NEON"]
        NUMA["NUMA placement"]
        MPI["MPI essential trees"]
    end
    G --> I
    B --> I
    P --> G
    P --> B
    SC --> G
    SC --> B
    SC --> R
    SC --> UI
    SC --> INS
    OMP --> G
    OMP --> B
    SIMD --> G
    MPI --> B
```

## What this is

N-Body Sim Pro is a serious, readable, CPU-only scientific computing engine: a
correctness-first reference implementation, progressively optimized with
OpenMP, runtime-dispatched SIMD, a Morton-order Barnes-Hut octree, NUMA-aware
placement, and MPI-distributed essential-tree execution 鈥?wrapped in a
technical SDL3/OpenGL/Dear ImGui workstation application and a headless,
benchmark-first CLI.

- **C17** for numerical kernels and low-level systems, **C++20** for the
  application, orchestration, and UI.
- **Physics is CPU-only.** OpenGL is used strictly for visualization.
- **Reference first.** A deliberately simple scalar O(N虏) kernel is the
  correctness authority; every optimization is validated against it.
- **Measured, not claimed.** Every performance number in this site was
  actually measured on real hardware and is reproducible with the
  `benchmark` command.

## Status sheet

| Component | Status | Notes |
|-----------|--------|-------|
| Reference O(N虏) gravity | 鉁?| scalar, double precision, single-threaded |
| Integrators | 鉁?| Euler, leapfrog (kick-drift-kick), velocity Verlet |
| Two-body regression | 鉁?| orbit, energy, momentum, COM over 8 orbits |
| Presets | 鉁?| 9 astrophysical presets, deterministic seeds |
| OpenMP | 鉁?| configurable threads, bit-identical all-pairs |
| SIMD dispatch | 鉁?| scalar / SSE2 / AVX2 (+FMA) / AVX-512 / NEON detected |
| AVX2 all-pairs kernel | 鉁?| 4.6脳 over scalar (measured) |
| SIMD Barnes-Hut | 鉁?| implemented, validated; measured slower than scalar |
| Barnes-Hut octree | 鉁?| contiguous nodes, Morton reorder, 胃 control |
| Allocation tracking | 鉁?| thread-local buffered events |
| Structured logging | 鉁?| levels, categories, developer console |
| Headless CLI + JSON | 鉁?| `benchmark`, `hardware`, `save`, `resume` |
| Checkpointing | 鉁?| versioned binary format, `save`/`resume` |
| NUMA | 鉁?| detection, affinity, first-touch generation |
| MPI distributed | 鉁?| essential-tree exchange, per-rank telemetry |
| 10M+ interactive | 鉁?| 17.5 s/step on the reference laptop (16 threads) |
| AVX-512 / NEON kernels | 鈻?| detected; not yet implemented |
| GPU physics | 鉁?| intentionally never |

## Headline measurements

Reproduced on an **Intel Core Ultra 7 255H** (16 threads, AVX2), Release build.
Full tables live in [Performance](/performance).

| Workload | Measured result |
|----------|-----------------|
| 16,384 particles, AVX2 vs scalar (1 thread) | **4.6脳** |
| 16,384 particles, OpenMP+AVX2 (16 threads) | **26.9脳** |
| 65,536 particles, Barnes-Hut vs scalar (1 thread) | **238脳** |
| 1M particles, Barnes-Hut, 16 threads | **1.39 s / step** |
| 10M particles, Barnes-Hut, 16 threads | **17.5 s / step** |

## The two hard rules

1. **Correctness before optimization.** The reference kernel stays forever.
   Optimized kernels are tested against it with defined tolerances.
2. **No fabricated telemetry.** Unavailable metrics render as `N/A`, not as
   placeholder numbers. If a kernel is slower, it is documented as slower.

Continue with [Getting Started](/getting-started) to build and run the
engine, or read the [Architecture](/architecture) and [Physics](/physics)
references.
