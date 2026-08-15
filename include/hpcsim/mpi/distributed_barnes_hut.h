#ifndef HPCSIM_MPI_DISTRIBUTED_BARNES_HUT_H
#define HPCSIM_MPI_DISTRIBUTED_BARNES_HUT_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"
#include "hpcsim/mpi/mpi_runtime.h"
#include "hpcsim/physics/gravity.h"

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
 * The force function matches HpcsimForceFunction, so the existing integrator
 * drives it: `view` is this rank's local particle view and `context` is the
 * HpcsimDistributedSimulation.
 */

typedef struct HpcsimDistributedSimulation HpcsimDistributedSimulation;

/* Create a distributed simulation bound to the caller's MPI runtime. */
HpcsimDistributedSimulation* hpcsim_distributed_create(const HpcsimMpiRuntime* runtime,
                                                       HpcsimError* error);
void hpcsim_distributed_destroy(HpcsimDistributedSimulation* simulation);

void hpcsim_distributed_set_theta(HpcsimDistributedSimulation* simulation, double theta);
double hpcsim_distributed_theta(const HpcsimDistributedSimulation* simulation);

/*
 * Force kernel for use with hpcsim_integrator_advance (or directly). Builds
 * the local tree over `view` (this rank's particles), performs the essential
 * tree exchange, and writes accelerations for `view`'s particles.
 */
HpcsimStatus hpcsim_distributed_compute_acceleration(const HpcsimParticleSystemView* view,
                                                     const HpcsimGravity* gravity,
                                                     void* context, HpcsimError* error);

typedef struct HpcsimDistributedStats {
    int rank;
    int comm_size;
    size_t local_particles;
    size_t remote_cells;
    size_t essential_cells;
    int levels_exchanged;
    double communication_time_seconds;
    double computation_time_seconds;
} HpcsimDistributedStats;

/* Read statistics from the most recent force evaluation. 0 on success. */
int hpcsim_distributed_stats(const HpcsimDistributedSimulation* simulation,
                             HpcsimDistributedStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_MPI_DISTRIBUTED_BARNES_HUT_H */
