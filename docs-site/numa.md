# NUMA-aware Placement

NUMA = Non-Uniform Memory Access. On multi-socket systems each socket has
its own memory controller; a thread reading memory on a remote socket pays a
latency and bandwidth penalty. NUMA-aware placement keeps pages and the
threads that touch them on the same node.

On the single-node development machine (Intel Core Ultra 7 255H) NUMA is one
node, so placement is a no-op. The code path is real and does the right
thing on multi-socket systems; the UI and `hardware` command report the
detected node count honestly.

## What the engine implements

| Facility | API | Platform |
|----------|-----|----------|
| Topology detection | `n_body_sim_pro_numa_detect` | Windows `GetNuma*`, Linux sysfs |
| Thread affinity | `n_body_sim_pro_thread_affinity_pin` / `apply_policy` | `SetThreadAffinityMask`, `sched_setaffinity` |
| First-touch | `n_body_sim_pro_memory_first_touch` | touches one double per page |

### First-touch

Memory pages are assigned to the NUMA node of the thread that first touches
them. The engine's parallel preset generator writes each thread's slice of
the SoA arrays directly, which is simultaneously:

1. **parallel generation** (OpenMP over particles), and
2. **NUMA placement** (pages land on the node of the thread that will
   process them).

Each particle's randomness is derived deterministically from
`(seed, particle index)`, so the parallel generator reproduces the same
initial conditions for any thread count.

### Thread affinity policies

- **Auto**: no pinning.
- **Compact**: threads fill logical processors in order (maximizes cache
  sharing).
- **Spread**: one thread per NUMA node before reuse.

Pinning is best-effort; it may fail in restricted environments and the call
reports failure rather than crashing.

## Measured effect on the reference machine

| generator | 1M particles, 16 threads |
|-----------|--------------------------|
| sequential | 0.488 s |
| parallel first-touch | 0.433 s |

The ~12% speedup comes from parallelism; the first-touch placement itself
cannot improve a single-NUMA machine. On multi-socket hardware the placement
is where the win materializes. No multi-node claim is made without such
hardware to measure it on.

## Platform limitations

- Windows: `GetNumaProcessorNode` covers up to 255 processors per the API
  contract; larger machines require `GetNumaProcessorNodeEx`.
- Linux: node topology is read from `/sys/devices/system/node/possible`.
- Containers and restricted sandboxes may report a single node or fail
  detection; the engine falls back to "1 node" rather than fabricating data.
