#include "hpcsim/barnes_hut/barnes_hut.h"
#include "barnes_hut_internal.h"

#include "hpcsim/memory/allocator.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

static double wall_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

double hpcsim_barnes_hut_wall_time_seconds(void) {
    return wall_time_seconds();
}

enum { BARNES_HUT_MAX_TREE_DEPTH = 128 };

#define BARNES_HUT_DEFAULT_THETA 0.7

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
    tree->morton_keys = NULL;
    tree->permutation = NULL;
    tree->sort_workspace = NULL;
    tree->counting_workspace = NULL;
    tree->reordered_positions_x = NULL;
    tree->reordered_positions_y = NULL;
    tree->reordered_positions_z = NULL;
    tree->reordered_masses = NULL;
    tree->reordered_accelerations_x = NULL;
    tree->reordered_accelerations_y = NULL;
    tree->reordered_accelerations_z = NULL;
    memset(&tree->reordered_view, 0, sizeof(tree->reordered_view));
    tree->reordered_count = 0;
    memset(&tree->stats, 0, sizeof(tree->stats));
    return tree;
}

void hpcsim_barnes_hut_tree_destroy(HpcsimBarnesHutTree* tree) {
    if (tree == NULL) {
        return;
    }
    hpcsim_deallocate(tree->morton_keys, __FILE__, __LINE__);
    hpcsim_deallocate(tree->permutation, __FILE__, __LINE__);
    hpcsim_deallocate(tree->sort_workspace, __FILE__, __LINE__);
    hpcsim_deallocate(tree->counting_workspace, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_positions_x, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_positions_y, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_positions_z, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_masses, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_accelerations_x, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_accelerations_y, __FILE__, __LINE__);
    hpcsim_deallocate(tree->reordered_accelerations_z, __FILE__, __LINE__);
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

/* Quantize a coordinate into 21 bits within [center - half, center + half). */
static uint64_t morton_axis_code(double value, double center, double half_size) {
    double normalized = (value - center) / (2.0 * half_size) + 0.5;
    if (normalized < 0.0) {
        normalized = 0.0;
    } else if (normalized >= 1.0) {
        normalized = 1.0 - 1.0e-15;
    }
    return (uint64_t)(normalized * 2097152.0); /* 2^21 */
}

/* Interleave the three 21-bit axis codes into a 63-bit Z-order key. */
static uint64_t morton_key(uint64_t x, uint64_t y, uint64_t z) {
    uint64_t key = 0;
    for (int bit = 0; bit < 21; ++bit) {
        key |= ((x >> bit) & 1) << (3 * bit);
        key |= ((y >> bit) & 1) << (3 * bit + 1);
        key |= ((z >> bit) & 1) << (3 * bit + 2);
    }
    return key;
}

/*
 * LSD radix sort of `permutation` by morton_keys using 21-bit digits
 * (3 passes). Stable, O(N), cache-friendly; the counting workspace needs
 * 2^21 entries.
 */
static void radix_sort_morton_keys(HpcsimBarnesHutTree* tree, size_t count) {
    for (int pass = 0; pass < 3; ++pass) {
        const unsigned int shift = (unsigned int)(21 * pass);
        const size_t mask = (1u << 21) - 1;

        memset(tree->counting_workspace, 0, (1u << 21) * sizeof(size_t));
        for (size_t i = 0; i < count; ++i) {
            const size_t digit =
                (size_t)((tree->morton_keys[tree->permutation[i]] >> shift) & mask);
            ++tree->counting_workspace[digit];
        }
        size_t cumulative = 0;
        for (size_t digit = 0; digit < (1u << 21); ++digit) {
            const size_t digit_count = tree->counting_workspace[digit];
            tree->counting_workspace[digit] = cumulative;
            cumulative += digit_count;
        }
        for (size_t i = 0; i < count; ++i) {
            const size_t permutation = tree->permutation[i];
            const size_t digit =
                (size_t)((tree->morton_keys[permutation] >> shift) & mask);
            tree->sort_workspace[tree->counting_workspace[digit]++] = permutation;
        }
        size_t* temporary = tree->permutation;
        tree->permutation = tree->sort_workspace;
        tree->sort_workspace = temporary;
    }
}

static int ensure_reorder_workspace(HpcsimBarnesHutTree* tree, size_t count,
                                    HpcsimError* error) {
    if (count <= tree->reordered_count) {
        return 1;
    }
    tree->reordered_count = count;

    const size_t key_bytes = count * sizeof(uint64_t);
    const size_t index_bytes = count * sizeof(size_t);
    const size_t double_bytes = count * sizeof(double);

    uint64_t* keys = (uint64_t*)hpcsim_reallocate(tree->morton_keys, key_bytes, __FILE__,
                                                  __LINE__);
    size_t* permutation = (size_t*)hpcsim_reallocate(tree->permutation, index_bytes,
                                                     __FILE__, __LINE__);
    size_t* sort_workspace = (size_t*)hpcsim_reallocate(tree->sort_workspace, index_bytes,
                                                        __FILE__, __LINE__);
    size_t* counting = (size_t*)hpcsim_reallocate(tree->counting_workspace,
                                                  (1u << 21) * sizeof(size_t), __FILE__,
                                                  __LINE__);
    double* px = (double*)hpcsim_reallocate(tree->reordered_positions_x, double_bytes,
                                            __FILE__, __LINE__);
    double* py = (double*)hpcsim_reallocate(tree->reordered_positions_y, double_bytes,
                                            __FILE__, __LINE__);
    double* pz = (double*)hpcsim_reallocate(tree->reordered_positions_z, double_bytes,
                                            __FILE__, __LINE__);
    double* masses = (double*)hpcsim_reallocate(tree->reordered_masses, double_bytes,
                                                __FILE__, __LINE__);
    double* ax = (double*)hpcsim_reallocate(tree->reordered_accelerations_x, double_bytes,
                                            __FILE__, __LINE__);
    double* ay = (double*)hpcsim_reallocate(tree->reordered_accelerations_y, double_bytes,
                                            __FILE__, __LINE__);
    double* az = (double*)hpcsim_reallocate(tree->reordered_accelerations_z, double_bytes,
                                            __FILE__, __LINE__);

    if (keys == NULL || permutation == NULL || sort_workspace == NULL || counting == NULL ||
        px == NULL || py == NULL || pz == NULL || masses == NULL || ax == NULL ||
        ay == NULL || az == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "failed to allocate Barnes-Hut reorder workspace");
        return 0;
    }
    tree->morton_keys = keys;
    tree->permutation = permutation;
    tree->sort_workspace = sort_workspace;
    tree->counting_workspace = counting;
    tree->reordered_positions_x = px;
    tree->reordered_positions_y = py;
    tree->reordered_positions_z = pz;
    tree->reordered_masses = masses;
    tree->reordered_accelerations_x = ax;
    tree->reordered_accelerations_y = ay;
    tree->reordered_accelerations_z = az;
    return 1;
}

/*
 * Reorder the caller's particle arrays into Morton (Z-order) sequence.
 * `permutation[i]` is the original index of the particle that now sits at
 * reordered position i, so reordered_position[i] = view->position[permutation[i]].
 */
static int reorder_particles_by_morton(HpcsimBarnesHutTree* tree,
                                       const HpcsimParticleSystemView* view,
                                       HpcsimError* error) {
    const size_t count = view->particle_count;
    if (!ensure_reorder_workspace(tree, count, error)) {
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        tree->permutation[i] = i;
        const uint64_t x = morton_axis_code(view->positions_x[i], tree->root_center_x,
                                            tree->root_half_size);
        const uint64_t y = morton_axis_code(view->positions_y[i], tree->root_center_y,
                                            tree->root_half_size);
        const uint64_t z = morton_axis_code(view->positions_z[i], tree->root_center_z,
                                            tree->root_half_size);
        tree->morton_keys[i] = morton_key(x, y, z);
    }
    radix_sort_morton_keys(tree, count);

    for (size_t i = 0; i < count; ++i) {
        const size_t original = tree->permutation[i];
        tree->reordered_positions_x[i] = view->positions_x[original];
        tree->reordered_positions_y[i] = view->positions_y[original];
        tree->reordered_positions_z[i] = view->positions_z[original];
        tree->reordered_masses[i] = view->masses[original];
    }
    tree->reordered_view.particle_count = count;
    tree->reordered_view.positions_x = tree->reordered_positions_x;
    tree->reordered_view.positions_y = tree->reordered_positions_y;
    tree->reordered_view.positions_z = tree->reordered_positions_z;
    tree->reordered_view.masses = tree->reordered_masses;
    tree->reordered_view.accelerations_x = tree->reordered_accelerations_x;
    tree->reordered_view.accelerations_y = tree->reordered_accelerations_y;
    tree->reordered_view.accelerations_z = tree->reordered_accelerations_z;
    tree->reordered_view.velocities_x = NULL;
    tree->reordered_view.velocities_y = NULL;
    tree->reordered_view.velocities_z = NULL;
    return 1;
}

HpcsimStatus hpcsim_barnes_hut_build_tree(HpcsimBarnesHutTree* tree,
                               const HpcsimParticleSystemView* view,
                               HpcsimError* error) {
    const size_t particle_count = view->particle_count;

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

    /* Reorder particles by Morton key; the build and evaluation then read the
     * cache-friendly reordered arrays. */
    if (!reorder_particles_by_morton(tree, view, error)) {
        return hpcsim_error_failed(error) ? error->status : HPCSIM_STATUS_OUT_OF_MEMORY;
    }
    tree->build_view = &tree->reordered_view;
    tree->node_count = 0;

    /* Preallocate the full node capacity (2N-1 worst case) so insertion never
     * reallocates or memcpy's the node buffer during the build. */
    const size_t worst_case_nodes = particle_count >= 1 ? 2 * particle_count - 1 : 1;
    if (!ensure_node_capacity(tree, worst_case_nodes)) {
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

typedef struct TraversalEntry {
    int32_t node_index;
    double cell_center_x;
    double cell_center_y;
    double cell_center_z;
    double cell_half_size;
} TraversalEntry;

/*
 * Iterative force traversal for one query particle.
 *
 * An explicit stack replaces recursion (no per-node call overhead) and the
 * opening test uses squared quantities, avoiding a square root per visited
 * node: a cell is accepted when  cell_size^2 < theta^2 * distance^2.
 *
 * `approximations` and `exact_interactions` accumulate the traversal
 * statistics so the OpenMP loop can reduce per-thread counts without
 * synchronizing the shared tree.
 */
void hpcsim_barnes_hut_evaluate_particle_scalar(const HpcsimBarnesHutTree* tree,
                              const HpcsimParticleSystemView* view,
                              const HpcsimGravity* gravity, size_t query_particle,
                              double* acceleration_x, double* acceleration_y,
                              double* acceleration_z, size_t* approximations,
                              size_t* exact_interactions) {
    TraversalEntry stack[1024];
    int stack_size = 1;
    stack[0] = (TraversalEntry){0, tree->root_center_x, tree->root_center_y,
                                tree->root_center_z, tree->root_half_size};

    const double query_x = view->positions_x[query_particle];
    const double query_y = view->positions_y[query_particle];
    const double query_z = view->positions_z[query_particle];
    const double theta_squared = tree->theta * tree->theta;
    const double softening_squared = gravity->softening_squared;
    const double gravitational_constant = gravity->gravitational_constant;

    while (stack_size > 0) {
        const TraversalEntry entry = stack[--stack_size];
        const BarnesHutNode* node = &tree->nodes[entry.node_index];
        if (node->particle_count == 0) {
            continue;
        }

        if (node->particle_index != -1) {
            if ((size_t)node->particle_index != query_particle) {
                const size_t j = (size_t)node->particle_index;
                const double delta_x = view->positions_x[j] - query_x;
                const double delta_y = view->positions_y[j] - query_y;
                const double delta_z = view->positions_z[j] - query_z;
                const double distance_squared =
                    delta_x * delta_x + delta_y * delta_y + delta_z * delta_z +
                    softening_squared;
                const double inverse_distance_cubed =
                    1.0 / (distance_squared * sqrt(distance_squared));
                const double force_scale =
                    gravitational_constant * view->masses[j] * inverse_distance_cubed;
                *acceleration_x += force_scale * delta_x;
                *acceleration_y += force_scale * delta_y;
                *acceleration_z += force_scale * delta_z;
                ++(*exact_interactions);
            }
            continue;
        }

        const double delta_x = node->center_of_mass_x - query_x;
        const double delta_y = node->center_of_mass_y - query_y;
        const double delta_z = node->center_of_mass_z - query_z;
        const double distance_squared =
            delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
        const double cell_size_squared = 4.0 * entry.cell_half_size * entry.cell_half_size;

        if (distance_squared > 0.0 && cell_size_squared < theta_squared * distance_squared) {
            const double softened_squared = distance_squared + softening_squared;
            const double inverse_distance_cubed =
                1.0 / (softened_squared * sqrt(softened_squared));
            const double force_scale =
                gravitational_constant * node->total_mass * inverse_distance_cubed;
            *acceleration_x += force_scale * delta_x;
            *acceleration_y += force_scale * delta_y;
            *acceleration_z += force_scale * delta_z;
            ++(*approximations);
            continue;
        }

        const double child_half_size = 0.5 * entry.cell_half_size;
        const double child_offset = entry.cell_half_size * 0.5;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            if (stack_size >= 1024) {
                break;
            }
            const double child_center_x =
                entry.cell_center_x + ((child & 1) ? child_offset : -child_offset);
            const double child_center_y =
                entry.cell_center_y + ((child & 2) ? child_offset : -child_offset);
            const double child_center_z =
                entry.cell_center_z + ((child & 4) ? child_offset : -child_offset);
            stack[stack_size++] = (TraversalEntry){
                child_index, child_center_x, child_center_y, child_center_z,
                child_half_size};
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

    const double build_start = wall_time_seconds();
    HpcsimStatus status = hpcsim_barnes_hut_build_tree(tree, view, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    const double build_finish = wall_time_seconds();

    const size_t particle_count = view->particle_count;
    const HpcsimParticleSystemView* reordered = &tree->reordered_view;
    double* const accelerations_x = reordered->accelerations_x;
    double* const accelerations_y = reordered->accelerations_y;
    double* const accelerations_z = reordered->accelerations_z;

    size_t total_approximations = 0;
    size_t total_exact_interactions = 0;

    const double evaluation_start = wall_time_seconds();
#pragma omp parallel for schedule(static) reduction(+ : total_approximations, total_exact_interactions)
    for (long long i = 0; i < (long long)particle_count; ++i) {
        size_t approximations = 0;
        size_t exact_interactions = 0;
        double acceleration_x = 0.0;
        double acceleration_y = 0.0;
        double acceleration_z = 0.0;
        hpcsim_barnes_hut_evaluate_particle_scalar(tree, reordered, gravity, (size_t)i, &acceleration_x,
                          &acceleration_y, &acceleration_z, &approximations,
                          &exact_interactions);
        accelerations_x[i] = acceleration_x;
        accelerations_y[i] = acceleration_y;
        accelerations_z[i] = acceleration_z;
        total_approximations += approximations;
        total_exact_interactions += exact_interactions;
    }
    const double evaluation_finish = wall_time_seconds();

    hpcsim_barnes_hut_scatter_accelerations(tree, view);

    tree->stats.accepted_approximations = total_approximations;
    tree->stats.exact_interactions = total_exact_interactions;
    tree->stats.build_time_seconds = build_finish - build_start;
    tree->stats.evaluation_time_seconds = evaluation_finish - evaluation_start;
    return HPCSIM_STATUS_OK;
}

void hpcsim_barnes_hut_scatter_accelerations(const HpcsimBarnesHutTree* tree,
                                             const HpcsimParticleSystemView* view) {
    const size_t particle_count = view->particle_count;
    const double* const accelerations_x = tree->reordered_view.accelerations_x;
    const double* const accelerations_y = tree->reordered_view.accelerations_y;
    const double* const accelerations_z = tree->reordered_view.accelerations_z;
    for (size_t i = 0; i < particle_count; ++i) {
        const size_t original = tree->permutation[i];
        view->accelerations_x[original] = accelerations_x[i];
        view->accelerations_y[original] = accelerations_y[i];
        view->accelerations_z[original] = accelerations_z[i];
    }
}

int hpcsim_barnes_hut_tree_stats(const HpcsimBarnesHutTree* tree,
                                 HpcsimBarnesHutStats* stats) {
    if (tree == NULL || stats == NULL) {
        return 1;
    }
    *stats = tree->stats;
    return 0;
}
