#include "n_body_sim_pro/memory/memory_estimate.h"

#include <stdint.h>

/*
 * Memory layout constants (must stay in sync with the actual structures):
 *   - Particle storage: 10 double arrays (SoA), 64-byte aligned, plus the
 *     owning system struct.
 *   - Barnes-Hut tree:  up to 2N-1 nodes in one contiguous array; each node
 *     is 72 bytes (8 child indices + particle index + count + center of mass
 *     + total mass).
 */

#define N_BODY_SIM_PRO_ESTIMATE_PARTICLE_COMPONENTS 10
#define N_BODY_SIM_PRO_ESTIMATE_BARNES_HUT_NODE_BYTES 72

NBodySimProStatus n_body_sim_pro_memory_estimate_simulation(size_t particle_count,
                                               int barnes_hut_enabled,
                                               NBodySimProMemoryEstimate* estimate,
                                               NBodySimProError* error) {
    if (estimate == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "estimate output is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    const size_t bytes_per_particle =
        N_BODY_SIM_PRO_ESTIMATE_PARTICLE_COMPONENTS * sizeof(double);
    if (particle_count != 0 && particle_count > SIZE_MAX / bytes_per_particle) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OVERFLOW, __FILE__, __LINE__,
                         "particle storage size overflows size_t");
        return N_BODY_SIM_PRO_STATUS_OVERFLOW;
    }

    estimate->particle_bytes = particle_count * bytes_per_particle;
    estimate->tree_bytes = 0;
    if (barnes_hut_enabled) {
        const size_t maximum_nodes = particle_count >= 1 ? 2 * particle_count - 1 : 0;
        if (maximum_nodes > SIZE_MAX / N_BODY_SIM_PRO_ESTIMATE_BARNES_HUT_NODE_BYTES) {
            n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OVERFLOW, __FILE__, __LINE__,
                             "tree node storage size overflows size_t");
            return N_BODY_SIM_PRO_STATUS_OVERFLOW;
        }
        estimate->tree_bytes = maximum_nodes * N_BODY_SIM_PRO_ESTIMATE_BARNES_HUT_NODE_BYTES;
    }
    estimate->workspace_bytes = 0;
    estimate->total_bytes =
        estimate->particle_bytes + estimate->tree_bytes + estimate->workspace_bytes;
    return N_BODY_SIM_PRO_STATUS_OK;
}
