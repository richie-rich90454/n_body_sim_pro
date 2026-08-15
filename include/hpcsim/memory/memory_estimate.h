#ifndef HPCSIM_MEMORY_MEMORY_ESTIMATE_H
#define HPCSIM_MEMORY_MEMORY_ESTIMATE_H

#include "hpcsim/core/status.h"

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

typedef struct HpcsimMemoryEstimate {
    size_t particle_bytes;
    size_t tree_bytes;
    size_t workspace_bytes;
    size_t total_bytes;
} HpcsimMemoryEstimate;

/*
 * Estimate the memory a simulation of `particle_count` particles will need.
 * `barnes_hut_enabled` includes the octree node array (up to 2N-1 nodes).
 * Returns HPCSIM_STATUS_OK and fills *estimate.
 */
HpcsimStatus hpcsim_memory_estimate_simulation(size_t particle_count,
                                               int barnes_hut_enabled,
                                               HpcsimMemoryEstimate* estimate,
                                               HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_MEMORY_MEMORY_ESTIMATE_H */
