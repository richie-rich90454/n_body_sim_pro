#ifndef N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_INTERNAL_H
#define N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_INTERNAL_H

#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"

#include "../barnes_hut/barnes_hut_internal.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Private implementation interface shared by the scalar distributed kernel
 * and the SIMD distributed-traversal variants. Not installed; C engine
 * internals only.
 */

enum {
    DISTRIBUTED_MAX_LEVELS = 64,
    DISTRIBUTED_REMOTE_LEAF_MARKER = -2,
    DISTRIBUTED_KEPT_CAPACITY = 1048576,
    DISTRIBUTED_WALK_STACK = 1024
};

typedef struct ExchangeCell {
    double com_x;
    double com_y;
    double com_z;
    double total_mass;
    double cell_center_x;
    double cell_center_y;
    double cell_center_z;
    double cell_half_size;
    int32_t owner_rank;
    int32_t owner_node_index;
    int32_t parent_node_index;
    int32_t is_internal;
} ExchangeCell;

typedef struct KeptCell {
    ExchangeCell cell;
    int32_t remote_index;
} KeptCell;

/* A foreign cell that this rank rejected (needs finer detail). */
typedef struct RejectedParent {
    int32_t owner_rank;
    int32_t owner_node_index;
} RejectedParent;

struct NBodySimProDistributedSimulation {
    int rank;
    int comm_size;
    int mpi_available;
    NBodySimProBarnesHutTree* local_tree;
    BarnesHutNode* remote_nodes;
    size_t remote_count;
    size_t remote_capacity;
    int32_t* remote_root_indices;
    ExchangeCell* remote_root_cells;
    size_t remote_root_count;
    KeptCell* kept_cells;
    size_t kept_count;
    size_t kept_capacity;
    double theta;

    size_t essential_cells;
    int levels_exchanged;
    double communication_time_seconds;
    double computation_time_seconds;
};

/*
 * Per-particle force evaluation used by the distributed kernels. The scalar
 * reference and the SIMD staged traversals share this shape, so one driver
 * (`n_body_sim_pro_distributed_compute_impl`) drives them all.
 */
typedef void (*NBodySimProDistributedEvaluateFn)(
    const NBodySimProDistributedSimulation* simulation, const NBodySimProBarnesHutTree* tree,
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity,
    size_t query_particle, double* acceleration_x, double* acceleration_y,
    double* acceleration_z);

/*
 * ISA-specific force-accumulation operations used by the staged distributed
 * traversal. The traversal itself (node visits, opening decisions) is shared
 * and identical to the scalar kernel; only the per-group arithmetic differs.
 */
typedef struct NBodySimProDistributedSimdOps {
    int width;
    size_t accumulator_size;
    void (*init)(void* accumulator);
    void (*flush)(void* accumulator, const double* dx, const double* dy,
                  const double* dz, const double* mass, double softening_squared,
                  double gravitational_constant);
    void (*reduce)(const void* accumulator, double scalar_x, double scalar_y, double scalar_z,
                   double* acceleration_x, double* acceleration_y, double* acceleration_z);
} NBodySimProDistributedSimdOps;

/*
 * Drive a full distributed force evaluation with the given per-particle
 * evaluation: build the local tree, run the essential-tree exchange, build
 * the remote essential forest, evaluate every local particle with OpenMP,
 * scatter, and record timings. Used by the scalar and every SIMD variant.
 */
NBodySimProStatus n_body_sim_pro_distributed_compute_impl(
    NBodySimProDistributedSimulation* simulation, const NBodySimProParticleSystemView* view,
    const NBodySimProGravity* gravity, NBodySimProDistributedEvaluateFn evaluate,
    NBodySimProError* error);

/*
 * Staged per-particle distributed traversal.
 *
 * Identical node visits and opening decisions to the scalar kernel; the
 * accepted interactions are staged into `ops->width` slots and applied
 * vectorized via `ops->flush`. A partial trailing group and the final reduce
 * are handled with `ops->reduce` plus the shared scalar tail.
 */
void n_body_sim_pro_distributed_evaluate_particle_staged(
    const NBodySimProDistributedSimulation* simulation, const NBodySimProBarnesHutTree* tree,
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity,
    size_t query_particle, const NBodySimProDistributedSimdOps* ops, void* accumulator,
    double* acceleration_x, double* acceleration_y, double* acceleration_z);

#endif /* N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_INTERNAL_H */
