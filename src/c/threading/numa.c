#include "n_body_sim_pro/threading/numa.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#include <string.h>
#endif

/* Logical processor count (Windows). */
static int logical_processor_count_win(void) {
#if defined(_WIN32)
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return count == 0 ? 1 : (int)count;
#else
    return 1;
#endif
}

int n_body_sim_pro_numa_detect(NBodySimProNumaTopology* topology) {
    if (topology == NULL) {
        return 1;
    }
    topology->node_count = 1;
    topology->logical_processor_count = 1;
    topology->processor_node = NULL;

#if defined(_WIN32)
    ULONG highest_node = 0;
    if (GetNumaHighestNodeNumber(&highest_node) != 0) {
        topology->node_count = (int)highest_node + 1;
    }
    const int processor_count = logical_processor_count_win();
    topology->logical_processor_count = processor_count;
    topology->processor_node =
        (int*)malloc((size_t)processor_count * sizeof(int));
    if (topology->processor_node == NULL) {
        return 1;
    }
    for (int processor = 0; processor < processor_count && processor <= 255; ++processor) {
        UCHAR node = 0;
        if (GetNumaProcessorNode((UCHAR)processor, &node) == 0) {
            topology->processor_node[processor] = (int)node;
        } else {
            topology->processor_node[processor] = 0;
        }
    }
    return 0;
#else
    /* Linux: read /sys/devices/system/node/node*/cpulist. Fall back to a
     * single node when the filesystem interface is absent. */
    FILE* file = fopen("/sys/devices/system/node/possible", "r");
    if (file == NULL) {
        return 1;
    }
    int maximum_node = 0;
    if (fscanf(file, "%d-%d", &maximum_node, &maximum_node) != 2) {
        fclose(file);
        return 1;
    }
    fclose(file);
    topology->node_count = maximum_node + 1;
    topology->logical_processor_count = 1;
    topology->processor_node = NULL;
    return 0;
#endif
}

void n_body_sim_pro_numa_topology_destroy(NBodySimProNumaTopology* topology) {
    if (topology == NULL) {
        return;
    }
    free(topology->processor_node);
    topology->processor_node = NULL;
}

int n_body_sim_pro_numa_node_count(const NBodySimProNumaTopology* topology) {
    return topology == NULL ? 1 : topology->node_count;
}

void n_body_sim_pro_numa_topology_print(const NBodySimProNumaTopology* topology) {
    if (topology == NULL) {
        return;
    }
    if (topology->node_count <= 1) {
        return;
    }
    for (int node = 0; node < topology->node_count; ++node) {
        int count = 0;
        for (int processor = 0; processor < topology->logical_processor_count;
             ++processor) {
            if (topology->processor_node != NULL &&
                topology->processor_node[processor] == node) {
                ++count;
            }
        }
        printf("  NUMA node %d: %d logical processors\n", node, count);
    }
}

int n_body_sim_pro_thread_affinity_pin(int logical_processor) {
#if defined(_WIN32)
    if (logical_processor < 0 || logical_processor >= 64) {
        /* Multi-group affinity requires SetThreadGroupAffinity; logical
         * processors beyond 63 fall back to single-group masking. */
        return 1;
    }
    HANDLE thread = GetCurrentThread();
    if (SetThreadAffinityMask(thread, (DWORD_PTR)1 << logical_processor) == 0) {
        return 1;
    }
    return 0;
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((size_t)logical_processor, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        return 1;
    }
    return 0;
#endif
}

void n_body_sim_pro_thread_affinity_apply_policy(NBodySimProThreadAffinityPolicy policy,
                                         int thread_index) {
    if (policy == N_BODY_SIM_PRO_AFFINITY_AUTO || thread_index < 0) {
        return;
    }
    NBodySimProNumaTopology topology;
    if (n_body_sim_pro_numa_detect(&topology) != 0) {
        return;
    }
    int target = -1;
    if (policy == N_BODY_SIM_PRO_AFFINITY_COMPACT) {
        target = thread_index % topology.logical_processor_count;
    } else if (policy == N_BODY_SIM_PRO_AFFINITY_SPREAD) {
        /* Place one thread on each NUMA node first, then cycle. */
        const int node = thread_index % topology.node_count;
        for (int processor = 0; processor < topology.logical_processor_count;
             ++processor) {
            if (topology.processor_node != NULL &&
                topology.processor_node[processor] == node) {
                target = processor;
                break;
            }
        }
    }
    if (target >= 0) {
        n_body_sim_pro_thread_affinity_pin(target);
    }
    n_body_sim_pro_numa_topology_destroy(&topology);
}

void n_body_sim_pro_memory_first_touch(double* buffer, size_t count) {
    if (buffer == NULL || count == 0) {
        return;
    }
    const size_t page_bytes = 4096;
    const size_t stride = page_bytes / sizeof(double);
    for (size_t i = 0; i < count; i += stride) {
        buffer[i] = 0.0;
    }
}
