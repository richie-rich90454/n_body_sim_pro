#ifndef N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_H
#define N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_H

#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"
#include "n_body_sim_pro/mpi/mpi_runtime.h"
#include "n_body_sim_pro/physics/gravity.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Distributed Barnes-Hut (MPI + OpenMP + SIMD-capable force evaluation).
 *
 * Particles are partitioned across ranks as contiguous blocks. Each force
 * evaluation:
 *   1. builds a local Morton-ordered octree over this rank's particles,
 *   2. exchanges a "local essential tree": the coarse cells of every other
 *      rank's tree that this rank's particles actually traverse, refined
 *      level by level until no particle needs finer detail,
 *   3. evaluates every local particle against the local tree plus the
 *      remote essential cells (center-of-mass approximations at distance).
 *
 * The result is the same Barnes-Hut approximation a single rank would
 * compute for the same global particle set, distributed: each rank computes
 * forces only for the particles it owns. It is never a fake single-process
 * "distributed" run: every rank holds a distinct particle block and real
 * messages move the tree cells.
 *
 * The force function matches NBodySimProForceFunction, so the existing integrator
 * drives it: `view` is this rank's local particle view and `context` is the
 * NBodySimProDistributedSimulation.
 */

typedef struct NBodySimProDistributedSimulation NBodySimProDistributedSimulation;

/* Create a distributed simulation bound to the caller's MPI runtime. */
NBodySimProDistributedSimulation* n_body_sim_pro_distributed_create(const NBodySimProMpiRuntime* runtime,
                                                       NBodySimProError* error);
void n_body_sim_pro_distributed_destroy(NBodySimProDistributedSimulation* simulation);

void n_body_sim_pro_distributed_set_theta(NBodySimProDistributedSimulation* simulation, double theta);
double n_body_sim_pro_distributed_theta(const NBodySimProDistributedSimulation* simulation);

/*
 * Force kernel for use with n_body_sim_pro_integrator_advance (or directly). Builds
 * the local tree over `view` (this rank's particles), performs the essential
 * tree exchange, and writes accelerations for `view`'s particles.
 */
NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration(const NBodySimProParticleSystemView* view,
                                                      const NBodySimProGravity* gravity,
                                                      void* context, NBodySimProError* error);

/*
 * SIMD-accelerated variants of the distributed traversal.
 *
 * The essential-tree exchange is identical to the scalar variant; only the
 * per-particle force accumulation over the local tree and the remote
 * essential forest is staged and applied with vector FMA (AVX2: 4 lanes,
 * AVX-512: 8 lanes, NEON: 2 lanes). The traversal makes identical opening
 * decisions, so the result matches the scalar distributed kernel within
 * floating-point tolerance, not bit-for-bit. On machines without the ISA the
 * functions degrade to the scalar distributed kernel, so their symbols
 * always exist.
 */
NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx2(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

typedef struct NBodySimProDistributedStats {
    int rank;
    int comm_size;
    size_t local_particles;
    size_t remote_cells;
    size_t essential_cells;
    int levels_exchanged;
    double communication_time_seconds;
    double computation_time_seconds;
} NBodySimProDistributedStats;

/* Read statistics from the most recent force evaluation. 0 on success. */
int n_body_sim_pro_distributed_stats(const NBodySimProDistributedSimulation* simulation,
                             NBodySimProDistributedStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_MPI_DISTRIBUTED_BARNES_HUT_H */
