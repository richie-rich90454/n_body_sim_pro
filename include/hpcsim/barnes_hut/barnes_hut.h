#ifndef HPCSIM_BARNES_HUT_BARNES_HUT_H
#define HPCSIM_BARNES_HUT_BARNES_HUT_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"
#include "hpcsim/physics/gravity.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Barnes-Hut octree gravity approximation, O(N log N).
 *
 * The tree is stored in contiguous node arrays with integer child indices;
 * no node is heap-allocated individually. The tree is rebuilt from the
 * current particle positions at the start of every force evaluation, so the
 * tree object owns a reusable node buffer that grows as needed.
 *
 * The opening criterion accepts a cell when  cell_size / distance < theta,
 * where distance is to the cell's center of mass. Lower theta is more
 * accurate and slower; higher theta is faster and less accurate. The exact
 * all-pairs kernel remains the correctness reference; Barnes-Hut is
 * validated against it in the test suite.
 */

typedef struct HpcsimBarnesHutTree HpcsimBarnesHutTree;

typedef struct HpcsimBarnesHutStats {
    size_t node_count;
    size_t leaf_count;
    size_t internal_node_count;
    size_t accepted_approximations;
    size_t exact_interactions;
    size_t maximum_depth;
    double build_time_seconds;
    double evaluation_time_seconds;
} HpcsimBarnesHutStats;

/* Create a reusable Barnes-Hut force context. NULL on allocation failure. */
HpcsimBarnesHutTree* hpcsim_barnes_hut_tree_create(HpcsimError* error);

/* Release the tree and its buffers. NULL-safe. */
void hpcsim_barnes_hut_tree_destroy(HpcsimBarnesHutTree* tree);

/* The opening angle used by the next force evaluation. */
void hpcsim_barnes_hut_tree_set_theta(HpcsimBarnesHutTree* tree, double theta);
double hpcsim_barnes_hut_tree_theta(const HpcsimBarnesHutTree* tree);

/*
 * Force kernel for use with hpcsim_integrator_advance.
 *
 * `context` must point to a HpcsimBarnesHutTree. The tree is rebuilt from
 * the current positions and the force evaluation is parallelized over query
 * particles with OpenMP. The traversal is serial per particle: exact
 * interactions for leaves and accepted approximations for distant cells.
 * Results match the reference kernel within tolerance (not bit-for-bit).
 */
HpcsimStatus hpcsim_barnes_hut_compute_acceleration(const HpcsimParticleSystemView* view,
                                                    const HpcsimGravity* gravity,
                                                    void* context, HpcsimError* error);

/*
 * AVX2 SIMD variants of the Barnes-Hut force evaluation.
 *
 * Four Morton-adjacent query particles are walked through the tree together;
 * nodes accepted by all four lanes are applied as four vectorized softened
 * interactions with 256-bit FMA. Because the group descends when any lane
 * needs finer detail, the result matches the scalar Barnes-Hut kernel within
 * tolerance (not bit-for-bit). On CPUs without AVX2 these functions degrade
 * to the scalar kernel.
 */
HpcsimStatus hpcsim_barnes_hut_compute_acceleration_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, void* context,
    HpcsimError* error);

HpcsimStatus hpcsim_barnes_hut_compute_acceleration_openmp_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, void* context,
    HpcsimError* error);

/*
 * Read statistics from the most recent force evaluation. Returns 0 on
 * success, non-zero if `tree` or `stats` is NULL.
 */
int hpcsim_barnes_hut_tree_stats(const HpcsimBarnesHutTree* tree,
                                 HpcsimBarnesHutStats* stats);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_BARNES_HUT_BARNES_HUT_H */
