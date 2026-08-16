#include "n_body_sim_pro/barnes_hut/barnes_hut.h"

#include "../barnes_hut/barnes_hut_internal.h"

#include <math.h>
#include <stdint.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * NEON SIMD Barnes-Hut force evaluation (AArch64).
 *
 * AArch64 NEON provides 128-bit vectors holding two doubles, so the
 * register-staging kernel processes two interactions per slot. The scalar
 * kernel's exact traversal and acceptance decisions are kept (identical node
 * visits, so results agree with the scalar kernel to floating-point
 * reordering only) and the accepted/leaf interactions are applied with
 * two-lane fused multiply-add.
 *
 * As with the AVX2 variant, the walk remains memory-latency bound; the SIMD
 * work accelerates the per-interaction arithmetic. The scalar kernel remains
 * the default; the SIMD variant is available for experimentation and for
 * hardware where the arithmetic share of the cost is larger.
 */

static double horizontal_sum_2(float64x2_t values) {
    return vgetq_lane_f64(values, 0) + vgetq_lane_f64(values, 1);
}

static void flush_stage(float64x2_t* accumulator_x, float64x2_t* accumulator_y,
                        float64x2_t* accumulator_z, const double stage_dx[2],
                        const double stage_dy[2], const double stage_dz[2],
                        const double stage_mass[2], double softening_squared,
                        double gravitational_constant) {
    const float64x2_t delta_x = vld1q_f64(stage_dx);
    const float64x2_t delta_y = vld1q_f64(stage_dy);
    const float64x2_t delta_z = vld1q_f64(stage_dz);
    const float64x2_t mass = vld1q_f64(stage_mass);

    const float64x2_t distance_squared =
        vfmaq_f64(vfmaq_f64(vfmaq_f64(vdupq_n_f64(softening_squared), delta_x, delta_x),
                            delta_y, delta_y),
                  delta_z, delta_z);
    const float64x2_t distance = vsqrtq_f64(distance_squared);
    const float64x2_t inverse_distance_cubed =
        vdivq_f64(vdupq_n_f64(1.0), vmulq_f64(distance_squared, distance));
    const float64x2_t force_scale =
        vmulq_f64(vdupq_n_f64(gravitational_constant),
                  vmulq_f64(mass, inverse_distance_cubed));

    *accumulator_x = vfmaq_f64(*accumulator_x, force_scale, delta_x);
    *accumulator_y = vfmaq_f64(*accumulator_y, force_scale, delta_y);
    *accumulator_z = vfmaq_f64(*accumulator_z, force_scale, delta_z);
}

static void evaluate_particle_batched(const NBodySimProBarnesHutTree* tree,
                                      const NBodySimProParticleSystemView* view,
                                      const NBodySimProGravity* gravity, size_t query_particle,
                                      double* acceleration_x, double* acceleration_y,
                                      double* acceleration_z, size_t* approximations,
                                      size_t* exact_interactions) {
    typedef struct WalkEntry {
        int32_t node_index;
        double cell_center_x;
        double cell_center_y;
        double cell_center_z;
        double cell_half_size;
    } WalkEntry;

    WalkEntry stack[1024];
    int stack_size = 1;
    stack[0] = (WalkEntry){0, tree->root_center_x, tree->root_center_y,
                           tree->root_center_z, tree->root_half_size};

    double stage_dx[2];
    double stage_dy[2];
    double stage_dz[2];
    double stage_mass[2];
    int stage_size = 0;

    float64x2_t accumulator_x = vdupq_n_f64(0.0);
    float64x2_t accumulator_y = vdupq_n_f64(0.0);
    float64x2_t accumulator_z = vdupq_n_f64(0.0);
    double scalar_acceleration_x = 0.0;
    double scalar_acceleration_y = 0.0;
    double scalar_acceleration_z = 0.0;

    const double query_x = view->positions_x[query_particle];
    const double query_y = view->positions_y[query_particle];
    const double query_z = view->positions_z[query_particle];
    const double theta_squared = tree->theta * tree->theta;
    const double softening_squared = gravity->softening_squared;
    const double gravitational_constant = gravity->gravitational_constant;
    size_t local_approximations = 0;
    size_t local_exact_interactions = 0;

#define N_BODY_SIM_PRO_BH_NEON_STAGE_OR_SCALAR(d_x, d_y, d_z, mass)          \
    do {                                                              \
        if (stage_size >= 2) {                                        \
            flush_stage(&accumulator_x, &accumulator_y, &accumulator_z, \
                        stage_dx, stage_dy, stage_dz, stage_mass,      \
                        softening_squared, gravitational_constant);    \
            stage_size = 0;                                            \
        }                                                              \
        stage_dx[stage_size] = (d_x);                                  \
        stage_dy[stage_size] = (d_y);                                  \
        stage_dz[stage_size] = (d_z);                                  \
        stage_mass[stage_size] = (mass);                               \
        ++stage_size;                                                  \
    } while (0)

    while (stack_size > 0) {
        const WalkEntry entry = stack[--stack_size];
        const BarnesHutNode* node = &tree->nodes[entry.node_index];
        if (node->particle_count == 0) {
            continue;
        }

        if (node->particle_index != -1) {
            if ((size_t)node->particle_index != query_particle) {
                const size_t j = (size_t)node->particle_index;
                N_BODY_SIM_PRO_BH_NEON_STAGE_OR_SCALAR(view->positions_x[j] - query_x,
                                              view->positions_y[j] - query_y,
                                              view->positions_z[j] - query_z, view->masses[j]);
                ++local_exact_interactions;
            }
            continue;
        }

        const double d_x = node->center_of_mass_x - query_x;
        const double d_y = node->center_of_mass_y - query_y;
        const double d_z = node->center_of_mass_z - query_z;
        const double distance_squared = d_x * d_x + d_y * d_y + d_z * d_z;
        const double cell_size_squared = 4.0 * entry.cell_half_size * entry.cell_half_size;

        if (distance_squared > 0.0 &&
            cell_size_squared < theta_squared * distance_squared) {
            N_BODY_SIM_PRO_BH_NEON_STAGE_OR_SCALAR(d_x, d_y, d_z, node->total_mass);
            ++local_approximations;
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
            stack[stack_size++] = (WalkEntry){child_index, child_center_x, child_center_y,
                                              child_center_z, child_half_size};
        }
    }

    if (stage_size > 0) {
        if (stage_size == 2) {
            flush_stage(&accumulator_x, &accumulator_y, &accumulator_z, stage_dx,
                        stage_dy, stage_dz, stage_mass, softening_squared,
                        gravitational_constant);
        } else {
            for (int slot = 0; slot < stage_size; ++slot) {
                const double d_x = stage_dx[slot];
                const double d_y = stage_dy[slot];
                const double d_z = stage_dz[slot];
                const double distance_squared =
                    d_x * d_x + d_y * d_y + d_z * d_z + softening_squared;
                const double inverse_distance_cubed =
                    1.0 / (distance_squared * sqrt(distance_squared));
                const double force_scale =
                    gravitational_constant * stage_mass[slot] * inverse_distance_cubed;
                scalar_acceleration_x += force_scale * d_x;
                scalar_acceleration_y += force_scale * d_y;
                scalar_acceleration_z += force_scale * d_z;
            }
        }
    }

    *acceleration_x = horizontal_sum_2(accumulator_x) + scalar_acceleration_x;
    *acceleration_y = horizontal_sum_2(accumulator_y) + scalar_acceleration_y;
    *acceleration_z = horizontal_sum_2(accumulator_z) + scalar_acceleration_z;
    *approximations += local_approximations;
    *exact_interactions += local_exact_interactions;
#undef N_BODY_SIM_PRO_BH_NEON_STAGE_OR_SCALAR
}

static NBodySimProStatus evaluate_all(NBodySimProBarnesHutTree* tree,
                                 const NBodySimProParticleSystemView* view,
                                 const NBodySimProGravity* gravity, int parallel) {
    const size_t particle_count = view->particle_count;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    size_t total_approximations = 0;
    size_t total_exact_interactions = 0;

#pragma omp parallel for schedule(static) reduction(+ : total_approximations, total_exact_interactions) if (parallel)
    for (long long i = 0; i < (long long)particle_count; ++i) {
        size_t approximations = 0;
        size_t exact_interactions = 0;
        double acceleration_x = 0.0;
        double acceleration_y = 0.0;
        double acceleration_z = 0.0;
        evaluate_particle_batched(tree, view, gravity, (size_t)i, &acceleration_x,
                                  &acceleration_y, &acceleration_z, &approximations,
                                  &exact_interactions);
        accelerations_x[i] = acceleration_x;
        accelerations_y[i] = acceleration_y;
        accelerations_z[i] = acceleration_z;
        total_approximations += approximations;
        total_exact_interactions += exact_interactions;
    }

    tree->stats.accepted_approximations = total_approximations;
    tree->stats.exact_interactions = total_exact_interactions;
    return N_BODY_SIM_PRO_STATUS_OK;
}

static NBodySimProStatus evaluate_with_build(NBodySimProBarnesHutTree* tree,
                                        const NBodySimProParticleSystemView* view,
                                        const NBodySimProGravity* gravity, int parallel,
                                        NBodySimProError* error) {
    if (tree == NULL || view == NULL || gravity == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "tree, view, and gravity parameters must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    const double build_start = n_body_sim_pro_barnes_hut_wall_time_seconds();
    NBodySimProStatus status = n_body_sim_pro_barnes_hut_build_tree(tree, view, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    const double build_finish = n_body_sim_pro_barnes_hut_wall_time_seconds();

    const double evaluation_start = n_body_sim_pro_barnes_hut_wall_time_seconds();
    status = evaluate_all(tree, &tree->reordered_view, gravity, parallel);
    const double evaluation_finish = n_body_sim_pro_barnes_hut_wall_time_seconds();

    n_body_sim_pro_barnes_hut_scatter_accelerations(tree, view);
    tree->stats.build_time_seconds = build_finish - build_start;
    tree->stats.evaluation_time_seconds = evaluation_finish - evaluation_start;
    return status;
}

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return evaluate_with_build((NBodySimProBarnesHutTree*)context, view, gravity, 0, error);
}

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_openmp_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return evaluate_with_build((NBodySimProBarnesHutTree*)context, view, gravity, 1, error);
}

#else /* !__aarch64__ */

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_barnes_hut_compute_acceleration(view, gravity, context, error);
}

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_openmp_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_barnes_hut_compute_acceleration(view, gravity, context, error);
}

#endif /* __aarch64__ */
