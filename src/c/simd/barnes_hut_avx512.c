#include "n_body_sim_pro/barnes_hut/barnes_hut.h"

#include "../barnes_hut/barnes_hut_internal.h"

#include <math.h>
#include <stdint.h>

#if defined(__AVX512F__)
#include <immintrin.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * AVX-512 SIMD Barnes-Hut force evaluation.
 *
 * The direct widening of the AVX2 register-staging kernel to 512-bit lanes.
 * The scalar kernel's exact traversal and acceptance decisions are kept
 * (identical node visits, so results agree with the scalar kernel to
 * floating-point reordering only), but the accepted and leaf interactions
 * are collected into an eight-slot stage and evaluated with 512-bit FMA.
 *
 * As with the AVX2 variant, the walk remains memory-latency bound; the SIMD
 * work accelerates the per-interaction arithmetic. The scalar kernel remains
 * the default; the SIMD variant is available for experimentation and for
 * hardware where the arithmetic share of the cost is larger.
 */

static double horizontal_sum_8(__m512d values) {
    const __m256d lower = _mm512_castpd512_pd256(values);
    const __m256d upper = _mm512_extractf64x4_pd(values, 1);
    const __m256d sum = _mm256_add_pd(lower, upper);
    const __m128d lower128 = _mm256_castpd256_pd128(sum);
    const __m128d upper128 = _mm256_extractf128_pd(sum, 1);
    const __m128d sum128 = _mm_add_pd(lower128, upper128);
    return _mm_cvtsd_f64(sum128) + _mm_cvtsd_f64(_mm_shuffle_pd(sum128, sum128, 1));
}

static void flush_stage(__m512d* accumulator_x, __m512d* accumulator_y,
                        __m512d* accumulator_z, const double stage_dx[8],
                        const double stage_dy[8], const double stage_dz[8],
                        const double stage_mass[8], double softening_squared,
                        double gravitational_constant) {
    const __m512d delta_x = _mm512_loadu_pd(stage_dx);
    const __m512d delta_y = _mm512_loadu_pd(stage_dy);
    const __m512d delta_z = _mm512_loadu_pd(stage_dz);
    const __m512d mass = _mm512_loadu_pd(stage_mass);

    const __m512d distance_squared =
        _mm512_fmadd_pd(delta_x, delta_x,
                        _mm512_fmadd_pd(delta_y, delta_y,
                                        _mm512_fmadd_pd(delta_z, delta_z,
                                                        _mm512_set1_pd(softening_squared))));
    const __m512d distance = _mm512_sqrt_pd(distance_squared);
    const __m512d inverse_distance_cubed =
        _mm512_div_pd(_mm512_set1_pd(1.0), _mm512_mul_pd(distance_squared, distance));
    const __m512d force_scale =
        _mm512_mul_pd(_mm512_set1_pd(gravitational_constant),
                      _mm512_mul_pd(mass, inverse_distance_cubed));

    *accumulator_x = _mm512_fmadd_pd(force_scale, delta_x, *accumulator_x);
    *accumulator_y = _mm512_fmadd_pd(force_scale, delta_y, *accumulator_y);
    *accumulator_z = _mm512_fmadd_pd(force_scale, delta_z, *accumulator_z);
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

    double stage_dx[8];
    double stage_dy[8];
    double stage_dz[8];
    double stage_mass[8];
    int stage_size = 0;

    __m512d accumulator_x = _mm512_setzero_pd();
    __m512d accumulator_y = _mm512_setzero_pd();
    __m512d accumulator_z = _mm512_setzero_pd();
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

#define N_BODY_SIM_PRO_BH512_STAGE_OR_SCALAR(d_x, d_y, d_z, mass)            \
    do {                                                              \
        if (stage_size >= 8) {                                        \
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
                N_BODY_SIM_PRO_BH512_STAGE_OR_SCALAR(view->positions_x[j] - query_x,
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
            N_BODY_SIM_PRO_BH512_STAGE_OR_SCALAR(d_x, d_y, d_z, node->total_mass);
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
        if (stage_size == 8) {
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

    *acceleration_x = horizontal_sum_8(accumulator_x) + scalar_acceleration_x;
    *acceleration_y = horizontal_sum_8(accumulator_y) + scalar_acceleration_y;
    *acceleration_z = horizontal_sum_8(accumulator_z) + scalar_acceleration_z;
    *approximations += local_approximations;
    *exact_interactions += local_exact_interactions;
#undef N_BODY_SIM_PRO_BH512_STAGE_OR_SCALAR
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

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return evaluate_with_build((NBodySimProBarnesHutTree*)context, view, gravity, 0, error);
}

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return evaluate_with_build((NBodySimProBarnesHutTree*)context, view, gravity, 1, error);
}

#else /* !__AVX512F__ */

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_barnes_hut_compute_acceleration(view, gravity, context, error);
}

NBodySimProStatus n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_barnes_hut_compute_acceleration(view, gravity, context, error);
}

#endif /* __AVX512F__ */
