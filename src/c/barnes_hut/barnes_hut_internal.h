#ifndef N_BODY_SIM_PRO_BARNES_HUT_INTERNAL_H
#define N_BODY_SIM_PRO_BARNES_HUT_INTERNAL_H

#include "n_body_sim_pro/barnes_hut/barnes_hut.h"

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

struct NBodySimProBarnesHutTree {
    BarnesHutNode* nodes;
    size_t node_capacity;
    size_t node_count;
    double root_center_x;
    double root_center_y;
    double root_center_z;
    double root_half_size;
    double theta;
    const NBodySimProParticleSystemView* build_view;
    NBodySimProBarnesHutStats stats;

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
    NBodySimProParticleSystemView reordered_view;
    size_t reordered_count;
};

/*
 * Build the tree (including Morton reordering) from `view`. On success the
 * tree's reordered_view holds the Morton-ordered particles and its
 * permutation maps reordered position -> original index.
 */
NBodySimProStatus n_body_sim_pro_barnes_hut_build_tree(NBodySimProBarnesHutTree* tree,
                                          const NBodySimProParticleSystemView* view,
                                          NBodySimProError* error);

/*
 * Scalar per-particle evaluation over the tree's reordered view. Used as the
 * fallback for the SIMD traversal's tail and on CPUs without AVX2.
 */
void n_body_sim_pro_barnes_hut_evaluate_particle_scalar(
    const NBodySimProBarnesHutTree* tree, const NBodySimProParticleSystemView* view,
    const NBodySimProGravity* gravity, size_t query_particle, double* acceleration_x,
    double* acceleration_y, double* acceleration_z, size_t* approximations,
    size_t* exact_interactions);

/* Copy the tree's reordered accelerations back into the caller's view. */
void n_body_sim_pro_barnes_hut_scatter_accelerations(const NBodySimProBarnesHutTree* tree,
                                             const NBodySimProParticleSystemView* view);

/* Monotonic wall clock in seconds (shared by all force-evaluation variants). */
double n_body_sim_pro_barnes_hut_wall_time_seconds(void);

#endif /* N_BODY_SIM_PRO_BARNES_HUT_INTERNAL_H */
