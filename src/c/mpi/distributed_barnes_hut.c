#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"

#include "../barnes_hut/barnes_hut_internal.h"
#include "n_body_sim_pro/memory/allocator.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef N_BODY_SIM_PRO_HAVE_MPI
#include <mpi.h>
#endif

/*
 * Distributed Barnes-Hut with a local essential tree exchange.
 *
 * Each rank owns a contiguous particle block. Per force evaluation the rank
 * builds a local Morton-ordered tree, then exchanges the cells of every
 * other rank's tree that its particles would actually visit:
 *
 *   level 0: every rank broadcasts its root cell; every rank keeps every
 *            foreign root (each particle applies the opening test to it).
 *   level L: a rank keeps a foreign cell when it is the child of a foreign
 *            cell it rejected (needed finer detail) at level L-1, and
 *            requests the next level for cells it rejects now.
 *   stops   : when no rank rejects any cell (MPI_Allreduce), the remote
 *             essential forest is complete: every cell any particle would
 *             descend into has its children present.
 *
 * The remote cells are assembled into a compact octree (same node layout as
 * the local tree, with remote leaves marked by particle_index == -2), and
 * the local tree and the remote forest are walked sequentially with reset
 * traversal stacks. MPI_Allgatherv moves the cell summaries; MPI_Allreduce
 * terminates the iteration. The opening test for every cell, local or
 * remote, measures distance to the cell's center of mass, and the essential
 * criterion uses the same distance, so the tree the traversal descends into
 * always has its children present.
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

NBodySimProDistributedSimulation* n_body_sim_pro_distributed_create(const NBodySimProMpiRuntime* runtime,
                                                       NBodySimProError* error) {
    if (runtime == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "MPI runtime must not be null");
        return NULL;
    }
    NBodySimProDistributedSimulation* simulation = (NBodySimProDistributedSimulation*)n_body_sim_pro_allocate(
        sizeof(NBodySimProDistributedSimulation), 64, N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE,
        __FILE__, __LINE__);
    if (simulation == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "failed to allocate distributed simulation");
        return NULL;
    }
    memset(simulation, 0, sizeof(*simulation));
    simulation->rank = runtime->rank;
    simulation->comm_size = runtime->comm_size;
    simulation->mpi_available = runtime->available;
    simulation->theta = 0.7;
    simulation->local_tree = n_body_sim_pro_barnes_hut_tree_create(error);
    if (simulation->local_tree == NULL) {
        n_body_sim_pro_deallocate(simulation, __FILE__, __LINE__);
        return NULL;
    }
    return simulation;
}

void n_body_sim_pro_distributed_destroy(NBodySimProDistributedSimulation* simulation) {
    if (simulation == NULL) {
        return;
    }
    n_body_sim_pro_barnes_hut_tree_destroy(simulation->local_tree);
    n_body_sim_pro_deallocate(simulation->remote_nodes, __FILE__, __LINE__);
    n_body_sim_pro_deallocate(simulation->remote_root_indices, __FILE__, __LINE__);
    n_body_sim_pro_deallocate(simulation->remote_root_cells, __FILE__, __LINE__);
    n_body_sim_pro_deallocate(simulation->kept_cells, __FILE__, __LINE__);
    n_body_sim_pro_deallocate(simulation, __FILE__, __LINE__);
}

void n_body_sim_pro_distributed_set_theta(NBodySimProDistributedSimulation* simulation, double theta) {
    if (simulation != NULL) {
        simulation->theta = theta;
        n_body_sim_pro_barnes_hut_tree_set_theta(simulation->local_tree, theta);
    }
}

double n_body_sim_pro_distributed_theta(const NBodySimProDistributedSimulation* simulation) {
    return simulation == NULL ? 0.0 : simulation->theta;
}

/* ------------------------------------------------------------------ */
/* Local tree cell extraction                                          */
/* ------------------------------------------------------------------ */

static size_t extract_cells_at_level(const NBodySimProBarnesHutTree* tree, int level,
                                     ExchangeCell* output) {
    typedef struct DfsEntry {
        int32_t node_index;
        int32_t parent_node_index;
        int depth;
        double cell_center_x;
        double cell_center_y;
        double cell_center_z;
        double cell_half_size;
    } DfsEntry;

    DfsEntry stack[256];
    int stack_size = 1;
    stack[0] = (DfsEntry){0, -1, 0, tree->root_center_x, tree->root_center_y,
                          tree->root_center_z, tree->root_half_size};
    size_t written = 0;

    while (stack_size > 0) {
        const DfsEntry entry = stack[--stack_size];
        const BarnesHutNode* node = &tree->nodes[entry.node_index];
        if (node->particle_count == 0) {
            continue;
        }
        if (entry.depth == level) {
            if (output != NULL) {
                ExchangeCell* cell = &output[written];
                cell->com_x = node->center_of_mass_x;
                cell->com_y = node->center_of_mass_y;
                cell->com_z = node->center_of_mass_z;
                cell->total_mass = node->total_mass;
                cell->cell_center_x = entry.cell_center_x;
                cell->cell_center_y = entry.cell_center_y;
                cell->cell_center_z = entry.cell_center_z;
                cell->cell_half_size = entry.cell_half_size;
                cell->owner_rank = 0;
                cell->owner_node_index = (int32_t)entry.node_index;
                cell->parent_node_index = entry.parent_node_index;
                cell->is_internal = node->particle_index == -1 ? 1 : 0;
            }
            ++written;
            continue;
        }
        if (node->particle_index != -1) {
            continue;
        }
        const double child_half_size = 0.5 * entry.cell_half_size;
        const double child_offset = entry.cell_half_size * 0.5;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            if (stack_size >= 256) {
                break;
            }
            const double child_center_x =
                entry.cell_center_x + ((child & 1) ? child_offset : -child_offset);
            const double child_center_y =
                entry.cell_center_y + ((child & 2) ? child_offset : -child_offset);
            const double child_center_z =
                entry.cell_center_z + ((child & 4) ? child_offset : -child_offset);
            stack[stack_size++] = (DfsEntry){child_index, (int32_t)entry.node_index,
                                             entry.depth + 1, child_center_x, child_center_y,
                                             child_center_z, child_half_size};
        }
    }
    return written;
}

/* ------------------------------------------------------------------ */
/* Essential-tree exchange                                             */
/* ------------------------------------------------------------------ */

/* Nearest-particle distance from a query point to any particle in the local
 * tree, computed with bounding-cell pruning. */
static double nearest_particle_distance(const NBodySimProBarnesHutTree* tree,
                                        const NBodySimProParticleSystemView* view,
                                        double query_x, double query_y, double query_z) {
    typedef struct NnEntry {
        int32_t node_index;
        double cell_center_x;
        double cell_center_y;
        double cell_center_z;
        double cell_half_size;
    } NnEntry;

    NnEntry stack[256];
    int stack_size = 1;
    stack[0] = (NnEntry){0, tree->root_center_x, tree->root_center_y, tree->root_center_z,
                         tree->root_half_size};
    double best_squared = INFINITY;

    while (stack_size > 0) {
        const NnEntry entry = stack[--stack_size];
        const BarnesHutNode* node = &tree->nodes[entry.node_index];
        if (node->particle_count == 0) {
            continue;
        }
        const double delta_x = query_x - entry.cell_center_x;
        const double delta_y = query_y - entry.cell_center_y;
        const double delta_z = query_z - entry.cell_center_z;
        const double closest_x =
            fabs(delta_x) - entry.cell_half_size > 0.0 ? fabs(delta_x) - entry.cell_half_size
                                                       : 0.0;
        const double closest_y =
            fabs(delta_y) - entry.cell_half_size > 0.0 ? fabs(delta_y) - entry.cell_half_size
                                                       : 0.0;
        const double closest_z =
            fabs(delta_z) - entry.cell_half_size > 0.0 ? fabs(delta_z) - entry.cell_half_size
                                                       : 0.0;
        const double closest_squared =
            closest_x * closest_x + closest_y * closest_y + closest_z * closest_z;
        if (closest_squared >= best_squared) {
            continue;
        }
        if (node->particle_index != -1) {
            const size_t j = (size_t)node->particle_index;
            const double d_x = view->positions_x[j] - query_x;
            const double d_y = view->positions_y[j] - query_y;
            const double d_z = view->positions_z[j] - query_z;
            const double d_squared = d_x * d_x + d_y * d_y + d_z * d_z;
            if (d_squared < best_squared) {
                best_squared = d_squared;
            }
            continue;
        }
        const double child_half_size = 0.5 * entry.cell_half_size;
        const double child_offset = entry.cell_half_size * 0.5;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            if (stack_size >= 256) {
                break;
            }
            const double child_center_x =
                entry.cell_center_x + ((child & 1) ? child_offset : -child_offset);
            const double child_center_y =
                entry.cell_center_y + ((child & 2) ? child_offset : -child_offset);
            const double child_center_z =
                entry.cell_center_z + ((child & 4) ? child_offset : -child_offset);
            stack[stack_size++] = (NnEntry){child_index, child_center_x, child_center_y,
                                            child_center_z, child_half_size};
        }
    }
    return best_squared;
}

/* 1 when some local particle would descend into this cell (needs finer).
 * The traversal's opening test measures distance to the cell's center of
 * mass, so the essential criterion uses the same distance. */
static int cell_rejected(const NBodySimProBarnesHutTree* tree,
                         const NBodySimProParticleSystemView* view, double theta,
                         const ExchangeCell* cell) {
    const double minimum_distance_squared =
        nearest_particle_distance(tree, view, cell->com_x, cell->com_y, cell->com_z);
    const double cell_size_squared = 4.0 * cell->cell_half_size * cell->cell_half_size;
    return minimum_distance_squared <= cell_size_squared / (theta * theta);
}

static int append_kept_cell(NBodySimProDistributedSimulation* simulation,
                            const ExchangeCell* cell) {
    if (simulation->kept_count >= simulation->kept_capacity) {
        size_t new_capacity =
            simulation->kept_capacity == 0 ? 1024 : simulation->kept_capacity * 2;
        KeptCell* grown = (KeptCell*)n_body_sim_pro_reallocate(
            simulation->kept_cells, new_capacity * sizeof(KeptCell), __FILE__, __LINE__);
        if (grown == NULL) {
            return 0;
        }
        simulation->kept_cells = grown;
        simulation->kept_capacity = new_capacity;
    }
    simulation->kept_cells[simulation->kept_count].cell = *cell;
    ++simulation->kept_count;
    ++simulation->essential_cells;
    return 1;
}

static int exchange_essential_tree(NBodySimProDistributedSimulation* simulation,
                                   const NBodySimProParticleSystemView* view) {
#ifdef N_BODY_SIM_PRO_HAVE_MPI
    const NBodySimProBarnesHutTree* tree = simulation->local_tree;
    const double communication_start = n_body_sim_pro_mpi_wall_time();

    simulation->essential_cells = 0;
    simulation->kept_count = 0;
    simulation->levels_exchanged = 0;

    ExchangeCell my_root;
    extract_cells_at_level(tree, 0, &my_root);
    my_root.owner_rank = simulation->rank;

    ExchangeCell* root_buffer =
        (ExchangeCell*)malloc((size_t)simulation->comm_size * sizeof(ExchangeCell));
    size_t rejected_capacity = (size_t)simulation->comm_size * 8;
    size_t next_rejected_capacity = (size_t)simulation->comm_size * 8;
    RejectedParent* rejected_parents =
        (RejectedParent*)malloc(rejected_capacity * sizeof(RejectedParent));
    RejectedParent* next_rejected_parents =
        (RejectedParent*)malloc(next_rejected_capacity * sizeof(RejectedParent));
    if (root_buffer == NULL || rejected_parents == NULL || next_rejected_parents == NULL) {
        free(root_buffer);
        free(rejected_parents);
        free(next_rejected_parents);
        return 0;
    }
    int rejected_count = 0;

#define N_BODY_SIM_PRO_DIST_APPEND_REJECTED(destination, capacity, count, rank_, node_index_) \
    do {                                                                             \
        if ((size_t)(count) >= (capacity)) {                                         \
            size_t new_capacity = (capacity) * 2;                                    \
            RejectedParent* grown = (RejectedParent*)realloc(                        \
                (destination), new_capacity * sizeof(RejectedParent));               \
            if (grown == NULL) {                                                     \
                free(root_buffer);                                                   \
                free(rejected_parents);                                              \
                free(next_rejected_parents);                                         \
                return 0;                                                            \
            }                                                                        \
            (destination) = grown;                                                   \
            (capacity) = new_capacity;                                               \
        }                                                                            \
        (destination)[(count)].owner_rank = (rank_);                                 \
        (destination)[(count)].owner_node_index = (node_index_);                     \
        ++(count);                                                                   \
    } while (0)

    /* Level 0: exchange roots; every foreign root is kept. */
    MPI_Allgather(&my_root, (int)sizeof(ExchangeCell), MPI_BYTE, root_buffer,
                  (int)sizeof(ExchangeCell), MPI_BYTE, MPI_COMM_WORLD);

    int any_rejected = 0;
    for (int rank = 0; rank < simulation->comm_size; ++rank) {
        if (rank == simulation->rank) {
            continue;
        }
        const ExchangeCell* cell = &root_buffer[rank];
        if (!append_kept_cell(simulation, cell)) {
            free(root_buffer);
            free(rejected_parents);
            free(next_rejected_parents);
            return 0;
        }
        if (cell->is_internal && cell_rejected(tree, view, simulation->theta, cell)) {
            N_BODY_SIM_PRO_DIST_APPEND_REJECTED(rejected_parents, rejected_capacity, rejected_count, rank,
                                        cell->owner_node_index);
            any_rejected = 1;
        }
    }
    simulation->levels_exchanged = 1;
    MPI_Allreduce(MPI_IN_PLACE, &any_rejected, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    for (int level = 1; any_rejected && level < DISTRIBUTED_MAX_LEVELS; ++level) {
        /* Send this rank's cells at `level`; receivers filter by parent. */
        const size_t capacity_estimate = tree->node_count < 64 ? 64 : tree->node_count;
        ExchangeCell* local_level_cells =
            (ExchangeCell*)malloc(capacity_estimate * sizeof(ExchangeCell));
        if (local_level_cells == NULL) {
            free(root_buffer);
            free(rejected_parents);
            free(next_rejected_parents);
            return 0;
        }
        const int local_level_count =
            (int)extract_cells_at_level(tree, level, local_level_cells);
        for (int i = 0; i < local_level_count; ++i) {
            local_level_cells[i].owner_rank = simulation->rank;
        }

        /* Gather every rank's cell count, then the cells themselves. */
        int* all_counts = (int*)malloc((size_t)simulation->comm_size * sizeof(int));
        int* displacements =
            (int*)malloc((size_t)simulation->comm_size * sizeof(int));
        if (all_counts == NULL || displacements == NULL) {
            free(root_buffer);
            free(rejected_parents);
            free(next_rejected_parents);
            free(all_counts);
            free(displacements);
            free(local_level_cells);
            return 0;
        }
        MPI_Allgather(&local_level_count, 1, MPI_INT, all_counts, 1, MPI_INT,
                      MPI_COMM_WORLD);
        /* MPI_BYTE counts are in bytes. */
        const int cell_bytes = (int)sizeof(ExchangeCell);
        int total_bytes = 0;
        for (int rank = 0; rank < simulation->comm_size; ++rank) {
            all_counts[rank] *= cell_bytes;
            displacements[rank] = total_bytes;
            total_bytes += all_counts[rank];
        }
        const int total_count = total_bytes / cell_bytes;
        ExchangeCell* all_level_cells = (ExchangeCell*)malloc((size_t)total_bytes);
        if (all_level_cells == NULL) {
            free(root_buffer);
            free(rejected_parents);
            free(next_rejected_parents);
            free(all_counts);
            free(displacements);
            free(local_level_cells);
            return 0;
        }
        MPI_Allgatherv(local_level_cells, local_level_count * cell_bytes, MPI_BYTE,
                       all_level_cells, all_counts, displacements, MPI_BYTE,
                       MPI_COMM_WORLD);

        /* Keep cells whose parent this rank rejected; flag the ones it
         * rejects now for the next level. */
        int next_rejected_count = 0;
        int local_any_rejected = 0;
        for (int i = 0; i < total_count; ++i) {
            const ExchangeCell* cell = &all_level_cells[i];
            int parent_rejected = 0;
            for (int r = 0; r < rejected_count; ++r) {
                if (cell->owner_rank == rejected_parents[r].owner_rank &&
                    cell->parent_node_index == rejected_parents[r].owner_node_index) {
                    parent_rejected = 1;
                    break;
                }
            }
            if (!parent_rejected) {
                continue;
            }
            if (!append_kept_cell(simulation, cell)) {
                free(root_buffer);
                free(rejected_parents);
                free(next_rejected_parents);
                free(all_counts);
                free(displacements);
                free(local_level_cells);
                free(all_level_cells);
                return 0;
            }
            if (cell->is_internal && cell_rejected(tree, view, simulation->theta, cell)) {
                N_BODY_SIM_PRO_DIST_APPEND_REJECTED(next_rejected_parents, next_rejected_capacity, next_rejected_count,
                                            cell->owner_rank, cell->owner_node_index);
                local_any_rejected = 1;
            }
        }

        /* Swap instead of copying: the two buffers have independent
         * capacities, and a straight memcpy could overflow the smaller one. */
        RejectedParent* swap = rejected_parents;
        rejected_parents = next_rejected_parents;
        next_rejected_parents = swap;
        const size_t swap_capacity = rejected_capacity;
        rejected_capacity = next_rejected_capacity;
        next_rejected_capacity = swap_capacity;
        rejected_count = next_rejected_count;

        any_rejected = 0;
        MPI_Allreduce(&local_any_rejected, &any_rejected, 1, MPI_INT, MPI_MAX,
                      MPI_COMM_WORLD);
        ++simulation->levels_exchanged;

        free(all_counts);
        free(displacements);
        free(local_level_cells);
        free(all_level_cells);
    }

    simulation->communication_time_seconds = n_body_sim_pro_mpi_wall_time() - communication_start;
    free(root_buffer);
    free(rejected_parents);
    free(next_rejected_parents);
    return 1;
#else
    (void)simulation;
    (void)view;
    return 0;
#endif
}

/* ------------------------------------------------------------------ */
/* Build the remote essential tree                                     */
/* ------------------------------------------------------------------ */

static int ensure_remote_capacity(NBodySimProDistributedSimulation* simulation,
                                  size_t required) {
    if (required <= simulation->remote_capacity) {
        return 1;
    }
    size_t new_capacity = simulation->remote_capacity == 0 ? 64 : simulation->remote_capacity;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2) {
            return 0;
        }
        new_capacity *= 2;
    }
    BarnesHutNode* new_nodes = (BarnesHutNode*)n_body_sim_pro_reallocate(
        simulation->remote_nodes, new_capacity * sizeof(BarnesHutNode), __FILE__, __LINE__);
    if (new_nodes == NULL) {
        return 0;
    }
    simulation->remote_nodes = new_nodes;
    simulation->remote_capacity = new_capacity;
    return 1;
}

/* Build the remote octree from the kept cells (parents precede children). */
static int build_remote_tree(NBodySimProDistributedSimulation* simulation) {
    simulation->remote_count = 0;
    simulation->remote_root_count = 0;

    for (size_t k = 0; k < simulation->kept_count; ++k) {
        const ExchangeCell* cell = &simulation->kept_cells[k].cell;
        if (!ensure_remote_capacity(simulation, simulation->remote_count + 1)) {
            return 0;
        }
        BarnesHutNode* node = &simulation->remote_nodes[simulation->remote_count];
        memset(node, 0, sizeof(*node));
        for (int child = 0; child < 8; ++child) {
            node->child_indices[child] = -1;
        }
        node->particle_index = cell->is_internal ? -1 : DISTRIBUTED_REMOTE_LEAF_MARKER;
        node->particle_count = 1;
        node->center_of_mass_x = cell->com_x;
        node->center_of_mass_y = cell->com_y;
        node->center_of_mass_z = cell->com_z;
        node->total_mass = cell->total_mass;
        const int32_t remote_index = (int32_t)simulation->remote_count;
        simulation->kept_cells[k].remote_index = remote_index;
        ++simulation->remote_count;

        if (cell->parent_node_index == -1) {
            if (simulation->remote_root_count < (size_t)simulation->comm_size) {
                simulation->remote_root_indices[simulation->remote_root_count] =
                    remote_index;
                simulation->remote_root_cells[simulation->remote_root_count] = *cell;
                ++simulation->remote_root_count;
            }
            continue;
        }
        /* Attach to the parent remote node (already built this or earlier level). */
        for (size_t m = 0; m < k; ++m) {
            const KeptCell* parent = &simulation->kept_cells[m];
            if (parent->cell.owner_rank == cell->owner_rank &&
                parent->cell.owner_node_index == cell->parent_node_index) {
                BarnesHutNode* parent_node = &simulation->remote_nodes[parent->remote_index];
                int octant = 0;
                if (cell->cell_center_x > parent->cell.cell_center_x) octant |= 1;
                if (cell->cell_center_y > parent->cell.cell_center_y) octant |= 2;
                if (cell->cell_center_z > parent->cell.cell_center_z) octant |= 4;
                parent_node->child_indices[octant] = remote_index;
                break;
            }
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Traversal over local + remote trees                                 */
/* ------------------------------------------------------------------ */

static void evaluate_particle_distributed(const NBodySimProDistributedSimulation* simulation,
                                          const NBodySimProBarnesHutTree* tree,
                                          const NBodySimProParticleSystemView* view,
                                          const NBodySimProGravity* gravity, size_t query_particle,
                                          double* acceleration_x, double* acceleration_y,
                                          double* acceleration_z) {
    typedef struct WalkEntry {
        int32_t node_index;
        int is_remote;
        double cell_center_x;
        double cell_center_y;
        double cell_center_z;
        double cell_half_size;
    } WalkEntry;

    const double query_x = view->positions_x[query_particle];
    const double query_y = view->positions_y[query_particle];
    const double query_z = view->positions_z[query_particle];
    const double theta_squared = simulation->theta * simulation->theta;
    const double softening_squared = gravity->softening_squared;
    const double gravitational_constant = gravity->gravitational_constant;

    double result_x = 0.0;
    double result_y = 0.0;
    double result_z = 0.0;

    /*
     * The local tree and the remote essential forest are walked
     * sequentially, each with its own (reset) traversal stack. The remote
     * forest's frontier can be wide; sharing one stack with the local walk
     * would overflow it and silently drop work.
     */
    WalkEntry stack[DISTRIBUTED_WALK_STACK];
    int stack_size = 0;

    /* Phase 1: the local tree (identical to the scalar kernel's traversal). */
    stack[stack_size++] =
        (WalkEntry){0, 0, tree->root_center_x, tree->root_center_y, tree->root_center_z,
                    tree->root_half_size};
    while (stack_size > 0) {
        const WalkEntry entry = stack[--stack_size];
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
                result_x += force_scale * delta_x;
                result_y += force_scale * delta_y;
                result_z += force_scale * delta_z;
            }
            continue;
        }
        const double delta_x = node->center_of_mass_x - query_x;
        const double delta_y = node->center_of_mass_y - query_y;
        const double delta_z = node->center_of_mass_z - query_z;
        const double distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
        const double cell_size_squared = 4.0 * entry.cell_half_size * entry.cell_half_size;
        if (distance_squared > 0.0 &&
            cell_size_squared < theta_squared * distance_squared) {
            const double softened_squared = distance_squared + softening_squared;
            const double inverse_distance_cubed =
                1.0 / (softened_squared * sqrt(softened_squared));
            const double force_scale =
                gravitational_constant * node->total_mass * inverse_distance_cubed;
            result_x += force_scale * delta_x;
            result_y += force_scale * delta_y;
            result_z += force_scale * delta_z;
            continue;
        }
        const double child_half_size = 0.5 * entry.cell_half_size;
        const double child_offset = entry.cell_half_size * 0.5;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            if (stack_size >= DISTRIBUTED_WALK_STACK) {
                break;
            }
            const double child_center_x =
                entry.cell_center_x + ((child & 1) ? child_offset : -child_offset);
            const double child_center_y =
                entry.cell_center_y + ((child & 2) ? child_offset : -child_offset);
            const double child_center_z =
                entry.cell_center_z + ((child & 4) ? child_offset : -child_offset);
            stack[stack_size++] = (WalkEntry){child_index, 0, child_center_x, child_center_y,
                                              child_center_z, child_half_size};
        }
    }

    /* Phase 2: the remote essential forest. */
    stack_size = 0;
    for (size_t root = 0; root < simulation->remote_root_count; ++root) {
        const ExchangeCell* cell = &simulation->remote_root_cells[root];
        stack[stack_size++] =
            (WalkEntry){simulation->remote_root_indices[root], 1, cell->cell_center_x,
                        cell->cell_center_y, cell->cell_center_z, cell->cell_half_size};
    }
    while (stack_size > 0) {
        const WalkEntry entry = stack[--stack_size];
        const BarnesHutNode* node = &simulation->remote_nodes[entry.node_index];
        if (node->particle_count == 0) {
            continue;
        }
        if (node->particle_index != -1) {
            double particle_x;
            double particle_y;
            double particle_z;
            double particle_mass;
            if (node->particle_index == DISTRIBUTED_REMOTE_LEAF_MARKER) {
                particle_x = node->center_of_mass_x;
                particle_y = node->center_of_mass_y;
                particle_z = node->center_of_mass_z;
                particle_mass = node->total_mass;
            } else {
                const size_t j = (size_t)node->particle_index;
                particle_x = view->positions_x[j];
                particle_y = view->positions_y[j];
                particle_z = view->positions_z[j];
                particle_mass = view->masses[j];
            }
            const double delta_x = particle_x - query_x;
            const double delta_y = particle_y - query_y;
            const double delta_z = particle_z - query_z;
            const double distance_squared =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z +
                softening_squared;
            const double inverse_distance_cubed =
                1.0 / (distance_squared * sqrt(distance_squared));
            const double force_scale =
                gravitational_constant * particle_mass * inverse_distance_cubed;
            result_x += force_scale * delta_x;
            result_y += force_scale * delta_y;
            result_z += force_scale * delta_z;
            continue;
        }
        const double delta_x = node->center_of_mass_x - query_x;
        const double delta_y = node->center_of_mass_y - query_y;
        const double delta_z = node->center_of_mass_z - query_z;
        const double distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
        const double cell_size_squared = 4.0 * entry.cell_half_size * entry.cell_half_size;
        if (distance_squared > 0.0 &&
            cell_size_squared < theta_squared * distance_squared) {
            const double softened_squared = distance_squared + softening_squared;
            const double inverse_distance_cubed =
                1.0 / (softened_squared * sqrt(softened_squared));
            const double force_scale =
                gravitational_constant * node->total_mass * inverse_distance_cubed;
            result_x += force_scale * delta_x;
            result_y += force_scale * delta_y;
            result_z += force_scale * delta_z;
            continue;
        }
        const double child_half_size = 0.5 * entry.cell_half_size;
        const double child_offset = entry.cell_half_size * 0.5;
        for (int child = 0; child < 8; ++child) {
            const int32_t child_index = node->child_indices[child];
            if (child_index == -1) {
                continue;
            }
            if (stack_size >= DISTRIBUTED_WALK_STACK) {
                break;
            }
            const double child_center_x =
                entry.cell_center_x + ((child & 1) ? child_offset : -child_offset);
            const double child_center_y =
                entry.cell_center_y + ((child & 2) ? child_offset : -child_offset);
            const double child_center_z =
                entry.cell_center_z + ((child & 4) ? child_offset : -child_offset);
            stack[stack_size++] = (WalkEntry){child_index, 1, child_center_x, child_center_y,
                                              child_center_z, child_half_size};
        }
    }

    *acceleration_x = result_x;
    *acceleration_y = result_y;
    *acceleration_z = result_z;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration(const NBodySimProParticleSystemView* view,
                                                     const NBodySimProGravity* gravity,
                                                     void* context, NBodySimProError* error) {
    NBodySimProDistributedSimulation* simulation = (NBodySimProDistributedSimulation*)context;
    if (simulation == NULL || view == NULL || gravity == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "simulation, view, and gravity must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    const double computation_start = n_body_sim_pro_mpi_wall_time();
    NBodySimProStatus status =
        n_body_sim_pro_barnes_hut_build_tree(simulation->local_tree, view, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    const NBodySimProParticleSystemView* local_view = &simulation->local_tree->reordered_view;

    if (simulation->mpi_available && simulation->comm_size > 1) {
        if (simulation->kept_cells == NULL) {
            simulation->kept_cells = (KeptCell*)n_body_sim_pro_allocate(
                DISTRIBUTED_KEPT_CAPACITY * sizeof(KeptCell), 64,
                N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE, __FILE__, __LINE__);
            simulation->remote_root_indices = (int32_t*)n_body_sim_pro_allocate(
                (size_t)simulation->comm_size * sizeof(int32_t), 64,
                N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE, __FILE__, __LINE__);
            simulation->remote_root_cells = (ExchangeCell*)n_body_sim_pro_allocate(
                (size_t)simulation->comm_size * sizeof(ExchangeCell), 64,
                N_BODY_SIM_PRO_ALLOCATION_CATEGORY_THREAD_WORKSPACE, __FILE__, __LINE__);
        }
        if (simulation->kept_cells == NULL || simulation->remote_root_indices == NULL ||
            simulation->remote_root_cells == NULL) {
            n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                             "failed to allocate distributed exchange workspace");
            return N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY;
        }
        if (!exchange_essential_tree(simulation, local_view)) {
            n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                             "distributed essential tree exchange failed");
            return N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY;
        }
        if (!build_remote_tree(simulation)) {
            n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                             "failed to build remote essential tree");
            return N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY;
        }
    } else {
        simulation->remote_count = 0;
        simulation->remote_root_count = 0;
        simulation->essential_cells = 0;
        simulation->levels_exchanged = 0;
        simulation->communication_time_seconds = 0.0;
    }

    const size_t particle_count = local_view->particle_count;
#pragma omp parallel for schedule(static)
    for (long long i = 0; i < (long long)particle_count; ++i) {
        double acceleration_x = 0.0;
        double acceleration_y = 0.0;
        double acceleration_z = 0.0;
        evaluate_particle_distributed(simulation, simulation->local_tree, local_view,
                                      gravity, (size_t)i, &acceleration_x,
                                      &acceleration_y, &acceleration_z);
        local_view->accelerations_x[i] = acceleration_x;
        local_view->accelerations_y[i] = acceleration_y;
        local_view->accelerations_z[i] = acceleration_z;
    }

    n_body_sim_pro_barnes_hut_scatter_accelerations(simulation->local_tree, view);
    simulation->computation_time_seconds = n_body_sim_pro_mpi_wall_time() - computation_start;
    return N_BODY_SIM_PRO_STATUS_OK;
}

int n_body_sim_pro_distributed_stats(const NBodySimProDistributedSimulation* simulation,
                             NBodySimProDistributedStats* stats) {
    if (simulation == NULL || stats == NULL) {
        return 1;
    }
    stats->rank = simulation->rank;
    stats->comm_size = simulation->comm_size;
    stats->local_particles =
        simulation->local_tree != NULL ? simulation->local_tree->reordered_count : 0;
    stats->remote_cells = simulation->remote_count;
    stats->essential_cells = simulation->essential_cells;
    stats->levels_exchanged = simulation->levels_exchanged;
    stats->communication_time_seconds = simulation->communication_time_seconds;
    stats->computation_time_seconds = simulation->computation_time_seconds;
    return 0;
}
