#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"

#include "distributed_barnes_hut_internal.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__AVX2__)
#include <immintrin.h>

/*
 * AVX2 SIMD acceleration of the distributed Barnes-Hut traversal.
 *
 * The essential-tree exchange and the local tree build are byte-identical
 * to the scalar kernel; only the per-particle force accumulation over the
 * local tree and the remote essential forest is staged and applied with
 * 256-bit FMA. The staged traversal (distributed_barnes_hut_internal.h)
 * makes identical opening decisions, so the result matches the scalar
 * distributed kernel within floating-point tolerance, not bit-for-bit.
 */

typedef struct Avx2DistAccumulator {
    __m256d x;
    __m256d y;
    __m256d z;
} Avx2DistAccumulator;

static void distributed_avx2_init(void* accumulator) {
    Avx2DistAccumulator* acc = (Avx2DistAccumulator*)accumulator;
    acc->x = _mm256_setzero_pd();
    acc->y = _mm256_setzero_pd();
    acc->z = _mm256_setzero_pd();
}

static void distributed_avx2_flush(void* accumulator, const double* dx, const double* dy,
                                   const double* dz, const double* mass, double softening_squared,
                                   double gravitational_constant) {
    Avx2DistAccumulator* acc = (Avx2DistAccumulator*)accumulator;
    const __m256d delta_x = _mm256_loadu_pd(dx);
    const __m256d delta_y = _mm256_loadu_pd(dy);
    const __m256d delta_z = _mm256_loadu_pd(dz);
    const __m256d mass_vector = _mm256_loadu_pd(mass);

    const __m256d distance_squared =
        _mm256_fmadd_pd(delta_x, delta_x,
                        _mm256_fmadd_pd(delta_y, delta_y,
                                        _mm256_fmadd_pd(delta_z, delta_z,
                                                        _mm256_set1_pd(softening_squared))));
    const __m256d distance = _mm256_sqrt_pd(distance_squared);
    const __m256d inverse_distance_cubed =
        _mm256_div_pd(_mm256_set1_pd(1.0), _mm256_mul_pd(distance_squared, distance));
    const __m256d force_scale =
        _mm256_mul_pd(_mm256_set1_pd(gravitational_constant),
                      _mm256_mul_pd(mass_vector, inverse_distance_cubed));

    acc->x = _mm256_fmadd_pd(force_scale, delta_x, acc->x);
    acc->y = _mm256_fmadd_pd(force_scale, delta_y, acc->y);
    acc->z = _mm256_fmadd_pd(force_scale, delta_z, acc->z);
}

static double distributed_avx2_horizontal_sum(__m256d values) {
    const __m128d lower = _mm256_castpd256_pd128(values);
    const __m128d upper = _mm256_extractf128_pd(values, 1);
    const __m128d sum = _mm_add_pd(lower, upper);
    const __m128d shuffled = _mm_shuffle_pd(sum, sum, 1);
    return _mm_cvtsd_f64(_mm_add_pd(sum, shuffled));
}

static void distributed_avx2_reduce(const void* accumulator, double scalar_x, double scalar_y,
                                    double scalar_z, double* acceleration_x,
                                    double* acceleration_y, double* acceleration_z) {
    const Avx2DistAccumulator* acc = (const Avx2DistAccumulator*)accumulator;
    *acceleration_x = distributed_avx2_horizontal_sum(acc->x) + scalar_x;
    *acceleration_y = distributed_avx2_horizontal_sum(acc->y) + scalar_y;
    *acceleration_z = distributed_avx2_horizontal_sum(acc->z) + scalar_z;
}

static const NBodySimProDistributedSimdOps distributed_avx2_ops = {
    4,
    sizeof(Avx2DistAccumulator),
    distributed_avx2_init,
    distributed_avx2_flush,
    distributed_avx2_reduce,
};

static void evaluate_particle_distributed_avx2(const NBodySimProDistributedSimulation* simulation,
                                               const NBodySimProBarnesHutTree* tree,
                                               const NBodySimProParticleSystemView* view,
                                               const NBodySimProGravity* gravity,
                                               size_t query_particle, double* acceleration_x,
                                               double* acceleration_y, double* acceleration_z) {
    Avx2DistAccumulator accumulator;
    distributed_avx2_init(&accumulator);
    n_body_sim_pro_distributed_evaluate_particle_staged(
        simulation, tree, view, gravity, query_particle, &distributed_avx2_ops, &accumulator,
        acceleration_x, acceleration_y, acceleration_z);
}

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx2(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    NBodySimProDistributedSimulation* simulation = (NBodySimProDistributedSimulation*)context;
    return n_body_sim_pro_distributed_compute_impl(
        simulation, view, gravity, evaluate_particle_distributed_avx2, error);
}

#else /* !__AVX2__ */

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx2(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_distributed_compute_acceleration(view, gravity, context, error);
}

#endif /* __AVX2__ */
