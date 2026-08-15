#ifndef N_BODY_SIM_PRO_THREADING_NUMA_H
#define N_BODY_SIM_PRO_THREADING_NUMA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NUMA topology, thread placement, and first-touch memory.
 *
 * NUMA-aware placement matters on multi-socket systems: each socket has its
 * own memory controller, and a thread reading memory on a remote socket
 * pays a latency/bandwidth penalty. The engine detects the topology, can pin
 * OpenMP threads to specific logical processors according to a placement
 * policy, and provides a first-touch helper so memory pages are established
 * on the node that will actually touch them.
 *
 * On systems with a single NUMA node (common on laptops and many desktops,
 * including the development machine) these facilities report one node and
 * are harmless no-ops for placement. This is documented, not faked.
 */

typedef struct NBodySimProNumaTopology {
    int node_count;
    int logical_processor_count;
    /* processor_node[p] = NUMA node owning logical processor p. */
    int* processor_node;
} NBodySimProNumaTopology;

/* Detects the NUMA topology; returns 0 on success, 1 when unavailable. */
int n_body_sim_pro_numa_detect(NBodySimProNumaTopology* topology);
void n_body_sim_pro_numa_topology_destroy(NBodySimProNumaTopology* topology);
int n_body_sim_pro_numa_node_count(const NBodySimProNumaTopology* topology);
/* Prints a human-readable summary. */
void n_body_sim_pro_numa_topology_print(const NBodySimProNumaTopology* topology);

typedef enum NBodySimProThreadAffinityPolicy {
    N_BODY_SIM_PRO_AFFINITY_AUTO = 0,    /* no explicit pinning; let the OS decide */
    N_BODY_SIM_PRO_AFFINITY_COMPACT,     /* threads fill logical processors in order */
    N_BODY_SIM_PRO_AFFINITY_SPREAD       /* one thread per NUMA node before reuse */
} NBodySimProThreadAffinityPolicy;

/* Pins the calling thread to a logical processor. 0 on success. */
int n_body_sim_pro_thread_affinity_pin(int logical_processor);

/* Pins the calling thread according to a policy and thread index. */
void n_body_sim_pro_thread_affinity_apply_policy(NBodySimProThreadAffinityPolicy policy,
                                         int thread_index);

/*
 * First-touch: touch one double per memory page of `buffer` so the OS assigns
 * the pages to the NUMA node of the calling thread. Call from the thread
 * that will process the data. `count` is the number of doubles.
 */
void n_body_sim_pro_memory_first_touch(double* buffer, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_THREADING_NUMA_H */
