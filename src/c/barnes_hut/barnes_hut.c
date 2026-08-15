#include "hpcsim/barnes_hut/barnes_hut.h"

#include "hpcsim/memory/allocator.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * Barnes-Hut octree in contiguous storage.
 *
 * Nodes live in one growable array; children are referenced by integer
 * indices (per spec, no per-node heap allocation). Nodes are appended in
 * creation order, so a node's children always have higher indices than the
 * node itself; this makes a reverse-index pass a valid post-order traversal
 * for the center-of-mass pass.
 *
 * A node is a leaf when it stores a particle index; an internal node stores
 * a center of mass and total mass; an empty node has particle_count 0.
 */

enum { BARNES_HUT_MAX_TREE_DEPTH = 128 };

#define BARNES_HUT_DEFAULT_THETA 0.7

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
};

HpcsimBarnesHutTree* hpcsim_barnes_hut_tree_create(HpcsimError* error) {
    HpcsimBarnesHutTree* tree = (HpcsimBarnesHutTree*)hpcsim_allocate(
        sizeof(HpcsimBarnesHutTree), 64, HPCSIM_ALLOCATION_CATEGORY_OCTREE_NODES,
        __FILE__, __LINE__);
    if (tree == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "failed to allocate Barnes-Hut tree context");
        return NULL;
    }
    tree->nodes = NULL;
    tree->node_capacity = 0;
    tree->node_count = 0;
    tree->root_center_x = 0.0;
    tree->root_center_y = 0.0;
    tree->root_center_z = 0.0;
    tree->root_half_size = 1.0;
    tree->theta = BARNES_HUT_DEFAULT_THETA;
    tree->build_view = NULL;
    memset(&tree->stats, 0, sizeof(tree->stats));
    return tree;
}

void hpcsim_barnes_hut_tree_destroy(HpcsimBarnesHutTree* tree) {
    if (tree == NULL) {
        return;
    }
    hpcsim_deallocate(tree->nodes, __FILE__, __LINE__);
    hpcsim_deallocate(tree, __FILE__, __LINE__);
}

void hpcsim_barnes_hut_tree_set_theta(HpcsimBarnesHutTree* tree, double theta) {
    if (tree != NULL) {
        tree->theta = theta;
    }
}

double hpcsim_barnes_hut_tree_theta(const HpcsimBarnesHutTree* tree) {
    return tree == NULL ? 0.0 : tree->theta;
}

static int ensure_node_capacity(HpcsimBarnesHutTree* tree, size_t required) {
    if (required <= tree->node_capacity) {
        return 1;
    }
    size_t new_capacity = tree->node_capacity == 0 ? 16 : tree->node_capacity;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2) {
            return 0;
        }
        new_capacity *= 2;
    }
    BarnesHutNode* new_nodes = (BarnesHutNode*)hpcsim_reallocate(
        tree->nodes, new_capacity * sizeof(BarnesHutNode), __FILE__, __LINE__);
    if (new_nodes == NULL) {
        return 0;
    }
    tree->nodes = new_nodes;
    tree->node_capacity = new_capacity;
    return 1;
}

static size_t append_empty_node(HpcsimBarnesHutTree* tree) {
    BarnesHutNode* node = &tree->nodes[tree->node_count];
    for (int child = 0; child < 8; ++child) {
        node->child_indices[child] = -1;
    }
    node->particle_index = -1;
    node->particle_count = 0;
    node->center_of_mass_x = 0.0;
    node->center_of_mass_y = 0.0;
    node->center_of_mass_z = 0.0;
    node->total_mass = 0.0;
    return tree->node_count++;
}

static int octant_of(const HpcsimParticleSystemView* view, size_t particle_index,
                     double cell_center_x, double cell_center_y, double cell_center_z) {
    int octant = 0;
    if (view->positions_x[particle_index] > cell_center_x) {
        octant |= 1;
    }
    if (view->positions_y[particle_index] > cell_center_y) {
        octant |= 2;
    }
    if (view->positions_z[particle_index] > cell_center_z) {
        octant |= 4;
    }
    return octant;
}

/* Recursive insertion. `depth` bounds degenerate cases (duplicate positions). */
static HpcsimStatus insert_particle(HpcsimBarnesHutTree* tree, size_t node_index,
                                    size_t particle_index, double cell_center_x,
                                    double cell_center_y, double cell_center_z,
                                    double cell_half_size, int depth, HpcsimError* error) {
    if (depth > BARNES_HUT_MAX_TREE_DEPTH) {
        hpcsim_error_set(error, HPCSIM_STATUS_OVERFLOW, __FILE__, __LINE__,
                         "Barnes-Hut tree exceeded maximum depth (duplicate positions?)");
        return HPCSIM_STATUS_OVERFLOW;
    }

    /* NOTE: tree->nodes may be reallocated by ensure_node_capacity below,
     * so the node pointer is re-fetched after every potential growth. */
    if (tree->nodes[node_index].particle_index != -1) {
        /* Leaf: subdivide to separate the existing particle from the new one. */
        const size_t old_particle_index = (size_t)tree->nodes[node_index].particle_index;
        const int old_octant = octant_of(tree->build_view, old_particle_index,
                                         cell_center_x, cell_center_y, cell_center_z);

        if (!ensure_node_capacity(tree, tree->node_count + 1)) {
            hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                             "Barnes-Hut tree node buffer exhausted");
            return HPCSIM_STATUS_OUT_OF_MEMORY;
        }
        tree->nodes[node_index].particle_index = -1;
        const size_t old_child = append_empty_node(tree);
        tree->nodes[node_index].child_indices[old_octant] = (int32_t)old_child;

        BarnesHutNode* old_node = &tree->nodes[old_child];
        old_node->particle_index = (int32_t)old_particle_index;
        old_node->particle_count = 1;
        old_node->total_mass = tree->build_view->masses[old_particle_index];
        old_node->center_of_mass_x = tree->build_view->positions_x[old_particle_index];
        old_node->center_of_mass_y = tree->build_view->positions_y[old_particle_index];
        old_node->center_of_mass_z = tree->build_view->positions_z[old_particle_index];
    }

    const int octant = octant_of(tree->build_view, particle_index, cell_center_x,
                                 cell_center_y, cell_center_z);
    int32_t child_index = tree->nodes[node_index].child_indices[octant];
    if (child_index == -1) {
        if (!ensure_node_capacity(tree, tree->node_count + 1)) {
            hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                             "Barnes-Hut tree node buffer exhausted");
            return HPCSIM_STATUS_OUT_OF_MEMORY;
        }
        child_index = (int32_t)append_empty_node(tree);
        tree->nodes[node_index].child_indices[octant] = child_index;
        BarnesHutNode* child = &tree->nodes[child_index];
        child->particle_index = (int32_t)particle_index;
        child->particle_count = 1;
        child->total_mass = tree->build_view->masses[particle_index];
        child->center_of_mass_x = tree->build_view->positions_x[particle_index];
        child->center_of_mass_y = tree->build_view->positions_y[particle_index];
        child->center_of_mass_z = tree->build_view->positions_z[particle_index];
        return HPCSIM_STATUS_OK;
    }

    const double child_half_size = 0.5 * cell_half_size;
    const double child_center_x =
        cell_center_x + ((octant & 1) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
    const double child_center_y =
        cell_center_y + ((octant & 2) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
    const double child_center_z =
        cell_center_z + ((octant & 4) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
    return insert_particle(tree, (size_t)child_index, particle_index, child_center_x,
                           child_center_y, child_center_z, child_half_size, depth + 1,
                           error);
}

static void compute_center_of_masses(HpcsimBarnesHutTree* tree) {
    /* Children always have higher indices, so reverse iteration is
     * post-order: internal nodes are finalized after all their children. */
    for (size_t index = tree->node_count; index > 0; --index) {
        BarnesHutNode* node = &tree->nodes[index - 1];
        if (node->particle_index != -1) {
            continue;
        }
        if (node->particle_count == 0) {
            continue;
        }
        double mass = 0.0;
        double com_x = 0.0;
        double com_y = 0.0;
        double com_z = 0.0;
        int count = 0;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            const BarnesHutNode* child_node = &tree->nodes[child_index];
            if (child_node->particle_count == 0) {
                continue;
            }
            mass += child_node->total_mass;
            com_x += child_node->total_mass * child_node->center_of_mass_x;
            com_y += child_node->total_mass * child_node->center_of_mass_y;
            com_z += child_node->total_mass * child_node->center_of_mass_z;
            count += child_node->particle_count;
        }
        node->total_mass = mass;
        node->particle_count = count;
        if (mass != 0.0) {
            node->center_of_mass_x = com_x / mass;
            node->center_of_mass_y = com_y / mass;
            node->center_of_mass_z = com_z / mass;
        }
    }
}

static void count_statistics(HpcsimBarnesHutTree* tree) {
    tree->stats.node_count = tree->node_count;
    tree->stats.leaf_count = 0;
    tree->stats.internal_node_count = 0;
    for (size_t index = 0; index < tree->node_count; ++index) {
        const BarnesHutNode* node = &tree->nodes[index];
        if (node->particle_index != -1) {
            ++tree->stats.leaf_count;
        } else if (node->particle_count > 0) {
            ++tree->stats.internal_node_count;
        }
    }
}

static size_t compute_maximum_depth(const HpcsimBarnesHutTree* tree, size_t node_index) {
    const BarnesHutNode* node = &tree->nodes[node_index];
    size_t maximum_child_depth = 0;
    for (int child = 0; child < 8; ++child) {
        const int32_t child_index = node->child_indices[child];
        if (child_index == -1) {
            continue;
        }
        const size_t child_depth = compute_maximum_depth(tree, (size_t)child_index);
        if (child_depth > maximum_child_depth) {
            maximum_child_depth = child_depth;
        }
    }
    return maximum_child_depth + 1;
}

static HpcsimStatus build_tree(HpcsimBarnesHutTree* tree,
                               const HpcsimParticleSystemView* view,
                               HpcsimError* error) {
    const size_t particle_count = view->particle_count;
    tree->build_view = view;
    tree->node_count = 0;

    if (particle_count == 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "cannot build a Barnes-Hut tree for an empty system");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    double minimum_x = view->positions_x[0];
    double minimum_y = view->positions_y[0];
    double minimum_z = view->positions_z[0];
    double maximum_x = minimum_x;
    double maximum_y = minimum_y;
    double maximum_z = minimum_z;
    for (size_t i = 1; i < particle_count; ++i) {
        if (view->positions_x[i] < minimum_x) minimum_x = view->positions_x[i];
        if (view->positions_y[i] < minimum_y) minimum_y = view->positions_y[i];
        if (view->positions_z[i] < minimum_z) minimum_z = view->positions_z[i];
        if (view->positions_x[i] > maximum_x) maximum_x = view->positions_x[i];
        if (view->positions_y[i] > maximum_y) maximum_y = view->positions_y[i];
        if (view->positions_z[i] > maximum_z) maximum_z = view->positions_z[i];
    }

    double half_extent_x = 0.5 * (maximum_x - minimum_x);
    double half_extent_y = 0.5 * (maximum_y - minimum_y);
    double half_extent_z = 0.5 * (maximum_z - minimum_z);
    double half_size = half_extent_x;
    if (half_extent_y > half_size) half_size = half_extent_y;
    if (half_extent_z > half_size) half_size = half_extent_z;
    if (half_size == 0.0) {
        half_size = 0.5;
    }
    /* Expand so particles sitting exactly on the boundary stay in octants. */
    half_size *= 1.0 + 1.0e-9;

    tree->root_center_x = 0.5 * (minimum_x + maximum_x);
    tree->root_center_y = 0.5 * (minimum_y + maximum_y);
    tree->root_center_z = 0.5 * (minimum_z + maximum_z);
    tree->root_half_size = half_size;

    if (!ensure_node_capacity(tree, 1)) {
        hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "Barnes-Hut tree node buffer exhausted");
        return HPCSIM_STATUS_OUT_OF_MEMORY;
    }
    append_empty_node(tree);

    for (size_t i = 0; i < particle_count; ++i) {
        if (tree->nodes[0].particle_count == 0) {
            BarnesHutNode* root = &tree->nodes[0];
            root->particle_index = (int32_t)i;
            root->particle_count = 1;
            root->total_mass = view->masses[i];
            root->center_of_mass_x = view->positions_x[i];
            root->center_of_mass_y = view->positions_y[i];
            root->center_of_mass_z = view->positions_z[i];
            continue;
        }
        HpcsimStatus status = insert_particle(
            tree, 0, i, tree->root_center_x, tree->root_center_y, tree->root_center_z,
            half_size, 0, error);
        if (status != HPCSIM_STATUS_OK) {
            return status;
        }
    }

    compute_center_of_masses(tree);
    count_statistics(tree);
    tree->stats.maximum_depth = compute_maximum_depth(tree, 0);
    tree->stats.accepted_approximations = 0;
    tree->stats.exact_interactions = 0;
    return HPCSIM_STATUS_OK;
}

/*
 * Recursive force traversal for one query particle.
 *
 * `approximations` and `exact_interactions` accumulate the traversal
 * statistics so the OpenMP loop can reduce per-thread counts without
 * synchronizing the shared tree.
 */
static void traverse_node(const HpcsimBarnesHutTree* tree,
                          const HpcsimParticleSystemView* view,
                          const HpcsimGravity* gravity, size_t node_index,
                          double cell_center_x, double cell_center_y, double cell_center_z,
                          double cell_half_size, size_t query_particle,
                          double* acceleration_x, double* acceleration_y,
                          double* acceleration_z, size_t* approximations,
                          size_t* exact_interactions) {
    const BarnesHutNode* node = &tree->nodes[node_index];
    if (node->particle_count == 0) {
        return;
    }

    if (node->particle_index != -1) {
        if ((size_t)node->particle_index != query_particle) {
            const size_t j = (size_t)node->particle_index;
            const double delta_x = view->positions_x[j] - view->positions_x[query_particle];
            const double delta_y = view->positions_y[j] - view->positions_y[query_particle];
            const double delta_z = view->positions_z[j] - view->positions_z[query_particle];
            const double distance_squared =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z +
                gravity->softening_squared;
            const double inverse_distance_cubed =
                1.0 / (distance_squared * sqrt(distance_squared));
            const double force_scale =
                gravity->gravitational_constant * view->masses[j] * inverse_distance_cubed;
            *acceleration_x += force_scale * delta_x;
            *acceleration_y += force_scale * delta_y;
            *acceleration_z += force_scale * delta_z;
            ++(*exact_interactions);
        }
        return;
    }

    const double delta_x = node->center_of_mass_x - view->positions_x[query_particle];
    const double delta_y = node->center_of_mass_y - view->positions_y[query_particle];
    const double delta_z = node->center_of_mass_z - view->positions_z[query_particle];
    const double distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
    const double cell_size = 2.0 * cell_half_size;

    int descend = 1;
    if (distance_squared > 0.0) {
        const double distance = sqrt(distance_squared);
        if (cell_size / distance < tree->theta) {
            descend = 0;
            const double softened_squared =
                distance_squared + gravity->softening_squared;
            const double inverse_distance_cubed =
                1.0 / (softened_squared * sqrt(softened_squared));
            const double force_scale =
                gravity->gravitational_constant * node->total_mass * inverse_distance_cubed;
            *acceleration_x += force_scale * delta_x;
            *acceleration_y += force_scale * delta_y;
            *acceleration_z += force_scale * delta_z;
            ++(*approximations);
        }
    }

    if (descend) {
        const double child_half_size = 0.5 * cell_half_size;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            const double child_center_x =
                cell_center_x + ((child & 1) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
            const double child_center_y =
                cell_center_y + ((child & 2) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
            const double child_center_z =
                cell_center_z + ((child & 4) ? cell_half_size * 0.5 : -cell_half_size * 0.5);
            traverse_node(tree, view, gravity, (size_t)child_index, child_center_x,
                          child_center_y, child_center_z, child_half_size, query_particle,
                          acceleration_x, acceleration_y, acceleration_z, approximations,
                          exact_interactions);
        }
    }
}

HpcsimStatus hpcsim_barnes_hut_compute_acceleration(const HpcsimParticleSystemView* view,
                                                    const HpcsimGravity* gravity,
                                                    void* context, HpcsimError* error) {
    HpcsimBarnesHutTree* tree = (HpcsimBarnesHutTree*)context;
    if (tree == NULL || view == NULL || gravity == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "tree, view, and gravity parameters must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    HpcsimStatus status = build_tree(tree, view, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }

    const size_t particle_count = view->particle_count;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    size_t total_approximations = 0;
    size_t total_exact_interactions = 0;

#pragma omp parallel for schedule(static) reduction(+ : total_approximations, total_exact_interactions)
    for (long long i = 0; i < (long long)particle_count; ++i) {
        size_t approximations = 0;
        size_t exact_interactions = 0;
        double acceleration_x = 0.0;
        double acceleration_y = 0.0;
        double acceleration_z = 0.0;
        traverse_node(tree, view, gravity, 0, tree->root_center_x, tree->root_center_y,
                      tree->root_center_z, tree->root_half_size, (size_t)i,
                      &acceleration_x, &acceleration_y, &acceleration_z, &approximations,
                      &exact_interactions);
        accelerations_x[i] = acceleration_x;
        accelerations_y[i] = acceleration_y;
        accelerations_z[i] = acceleration_z;
        total_approximations += approximations;
        total_exact_interactions += exact_interactions;
    }

    tree->stats.accepted_approximations = total_approximations;
    tree->stats.exact_interactions = total_exact_interactions;
    return HPCSIM_STATUS_OK;
}

int hpcsim_barnes_hut_tree_stats(const HpcsimBarnesHutTree* tree,
                                 HpcsimBarnesHutStats* stats) {
    if (tree == NULL || stats == NULL) {
        return 1;
    }
    *stats = tree->stats;
    return 0;
}
