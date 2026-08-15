# Command Line Reference

The single executable `n_body_sim_pro` dispatches on its first argument.
With no arguments it launches the interactive application.

```
n_body_sim_pro [command] [options]
```

| Command | Purpose |
|---------|---------|
| *(none)* | launch the interactive SDL3/OpenGL application |
| `hardware` | print detected CPU, SIMD, OpenMP, and NUMA information |
| `benchmark` | run headless force-kernel benchmarks, JSON output |
| `save FILE` | write a fresh preset as a checkpoint |
| `resume FILE` | continue a checkpoint headless |
| `distributed` | run distributed with MPI (launch via `mpiexec`) |
| `--help` / `-h` | print usage |

## hardware

```
n_body_sim_pro hardware
```

Prints the CPU brand string (from CPUID extended leaf), architecture, OpenMP
availability and thread count, the per-ISA detection results, the selected
SIMD backend, and the NUMA node count. Only detected capabilities are
reported; nothing is assumed.

Example output (abridged):

```
CPU            : Intel(R) Core(TM) Ultra 7 255H
Architecture   : x86-64
OpenMP         : enabled (16 threads available)
SIMD           :
  SSE2         : available
  AVX2         : available
  FMA          : available
  AVX-512      : unavailable
  NEON         : unavailable
Selected backend : AVX2
NUMA            : 1 node
```

## benchmark

```
n_body_sim_pro benchmark [--particles N] [--steps S] [--threads T]
  [--algorithm A] [--theta T] [--preset P]
```

Measures a single force evaluation over `steps` runs and prints per-thread
timings, speedup, and parallel efficiency, followed by a machine-readable
JSON line.

| Option | Default | Meaning |
|--------|---------|---------|
| `--particles N` | 65536 | particle count |
| `--steps S` | 100 | force evaluations to time |
| `--threads T[,T2,..]` | `1` | OpenMP thread counts to benchmark |
| `--algorithm A` | auto | `reference`, `openmp`, `avx2`, `openmp_avx2`, `barnes_hut`, `barnes_hut_avx2`, `barnes_hut_openmp_avx2` |
| `--theta T` | 0.7 | Barnes-Hut opening angle |
| `--preset P` | galaxy_collision | see presets below |

When `--algorithm` is omitted, all-pairs is chosen below 20,000 particles and
Barnes-Hut above.

The JSON output is designed for regression tracking:

```json
{"cpu":"Intel(R) Core(TM) Ultra 7 255H","simd":{"avx2":true,"avx512":false,"neon":false},
 "threads":8,"particles":16384,"algorithm":"barnes_hut","theta":0.70,"steps":20,
 "estimated_bytes":3669944,"avg_step_ms":64.102320,"first_step_ms":68.341800,
 "energy_drift":0.000002,"momentum_error":1.069654e-06}
```

## save

```
n_body_sim_pro save FILE [--particles N] [--preset P] [--seed S]
```

Generates a preset and writes it as a versioned binary checkpoint. The
checkpoint stores positions, velocities, accelerations, masses, simulation
time, timestep, integrator, Barnes-Hut θ, seed, and preset.

## resume

```
n_body_sim_pro resume FILE [--steps S] [--threads T]
```

Loads a checkpoint and continues it headless for `steps` timesteps, then
prints the average step time, energy drift, and momentum error.

## distributed

```
mpiexec -n P n_body_sim_pro distributed [--particles N] [--steps S] [--theta T] [--seed S]
```

Each rank partitions the global particle count into a contiguous block,
generates its block deterministically, and evaluates forces with the
distributed Barnes-Hut essential-tree exchange. Every rank prints its own
telemetry line:

```
Rank 0: particles=2048 remote_cells=2951 essential=2951 levels=8 compute=29.56% communication=10.23% avg_step=63.511 ms
```

Not running under `mpiexec`, the command fails with a clear message rather
than silently running single-rank.

## Preset names

`two_body`, `random_cloud`, `solar_system`, `open_cluster`,
`globular_cluster`, `spiral_galaxy`, `elliptical_galaxy`, `galaxy_collision`,
`triple_galaxy`.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | success |
| 1 | command error, allocation failure, or a test assertion failed |
