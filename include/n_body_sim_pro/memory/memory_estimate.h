#ifndef N_BODY_SIM_PRO_MEMORY_MEMORY_ESTIMATE_H
#define N_BODY_SIM_PRO_MEMORY_MEMORY_ESTIMATE_H

#include "n_body_sim_pro/core/status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Memory requirement estimation before allocation.
 *
 * A massive particle system must be checked against available memory before
 * it is allocated, so the engine fails gracefully instead of crashing. The
 * estimate mirrors the actual layout the engine uses (SoA particle storage,
 * contiguous Barnes-Hut node array) so it stays honest with what will be
 * allocated.
 */

typedef struct NBodySimProMemoryEstimate {
    size_t particle_bytes;
    size_t tree_bytes;
    size_t workspace_bytes;
    size_t total_bytes;
} NBodySimProMemoryEstimate;

/*
 * Estimate the memory a simulation of `particle_count` particles will need.
 * `barnes_hut_enabled` includes the octree node array (up to 2N-1 nodes).
 * Returns N_BODY_SIM_PRO_STATUS_OK and fills *estimate.
 */
NBodySimProStatus n_body_sim_pro_memory_estimate_simulation(size_t particle_count,
                                               int barnes_hut_enabled,
                                               NBodySimProMemoryEstimate* estimate,
                                               NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_MEMORY_MEMORY_ESTIMATE_H */
