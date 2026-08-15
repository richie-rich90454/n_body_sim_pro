#ifndef HPCSIM_BARNES_HUT_INTERNAL_H
#define HPCSIM_BARNES_HUT_INTERNAL_H

#include "hpcsim/barnes_hut/barnes_hut.h"

#include <stdint.h>

/*
 * Private implementation interface shared by the Barnes-Hut module and the
 * SIMD force-traversal variants. Not installed; C engine internals only.
 */

typedef struct BarnesHutNode {
    int32_t child_indices[8];
    int32_t particle_index;
    int32_t particle_count;
    double center_of_mass_x;
    double center_of_mass_y;
    double center_of_mass_z;
    double total_mass;
} BarnesHutNode;

struct HpcsimBarnesHutTree {
    BarnesHutNode* nodes;
    size_t node_capacity;
    size_t node_count;
    double root_center_x;
    double root_center_y;
    double root_center_z;
    double root_half_size;
    double theta;
    const HpcsimParticleSystemView* build_view;
    HpcsimBarnesHutStats stats;

    uint64_t* morton_keys;
    size_t* permutation;
    size_t* sort_workspace;
    size_t* counting_workspace;
    double* reordered_positions_x;
    double* reordered_positions_y;
    double* reordered_positions_z;
    double* reordered_masses;
    double* reordered_accelerations_x;
    double* reordered_accelerations_y;
    double* reordered_accelerations_z;
    HpcsimParticleSystemView reordered_view;
    size_t reordered_count;
};

/*
 * Build the tree (including Morton reordering) from `view`. On success the
 * tree's reordered_view holds the Morton-ordered particles and its
 * permutation maps reordered position -> original index.
 */
HpcsimStatus hpcsim_barnes_hut_build_tree(HpcsimBarnesHutTree* tree,
                                          const HpcsimParticleSystemView* view,
                                          HpcsimError* error);

/*
 * Scalar per-particle evaluation over the tree's reordered view. Used as the
 * fallback for the SIMD traversal's tail and on CPUs without AVX2.
 */
void hpcsim_barnes_hut_evaluate_particle_scalar(
    const HpcsimBarnesHutTree* tree, const HpcsimParticleSystemView* view,
    const HpcsimGravity* gravity, size_t query_particle, double* acceleration_x,
    double* acceleration_y, double* acceleration_z, size_t* approximations,
    size_t* exact_interactions);

/* Copy the tree's reordered accelerations back into the caller's view. */
void hpcsim_barnes_hut_scatter_accelerations(const HpcsimBarnesHutTree* tree,
                                             const HpcsimParticleSystemView* view);

/* Monotonic wall clock in seconds (shared by all force-evaluation variants). */
double hpcsim_barnes_hut_wall_time_seconds(void);

#endif /* HPCSIM_BARNES_HUT_INTERNAL_H */
