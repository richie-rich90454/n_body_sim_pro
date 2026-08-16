#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"

#include "distributed_barnes_hut_internal.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__AVX512F__)
#include <immintrin.h>

/*
 * AVX-512 SIMD acceleration of the distributed Barnes-Hut traversal.
 *
 * The direct widening of the AVX2 variant: eight staged interactions are
 * applied per 512-bit FMA group. The essential-tree exchange and the local
 * tree build are byte-identical to the scalar kernel; only the per-particle
 * force accumulation is vectorized, so the result matches the scalar
 * distributed kernel within floating-point tolerance.
 */

typedef struct Avx512DistAccumulator {
    __m512d x;
    __m512d y;
    __m512d z;
} Avx512DistAccumulator;

static void distributed_avx512_init(void* accumulator) {
    Avx512DistAccumulator* acc = (Avx512DistAccumulator*)accumulator;
    acc->x = _mm512_setzero_pd();
    acc->y = _mm512_setzero_pd();
    acc->z = _mm512_setzero_pd();
}

static void distributed_avx512_flush(void* accumulator, const double* dx, const double* dy,
                                     const double* dz, const double* mass, double softening_squared,
                                     double gravitational_constant) {
    Avx512DistAccumulator* acc = (Avx512DistAccumulator*)accumulator;
    const __m512d delta_x = _mm512_loadu_pd(dx);
    const __m512d delta_y = _mm512_loadu_pd(dy);
    const __m512d delta_z = _mm512_loadu_pd(dz);
    const __m512d mass_vector = _mm512_loadu_pd(mass);

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
                      _mm512_mul_pd(mass_vector, inverse_distance_cubed));

    acc->x = _mm512_fmadd_pd(force_scale, delta_x, acc->x);
    acc->y = _mm512_fmadd_pd(force_scale, delta_y, acc->y);
    acc->z = _mm512_fmadd_pd(force_scale, delta_z, acc->z);
}

static double distributed_avx512_horizontal_sum(__m512d values) {
    const __m256d lower = _mm512_castpd512_pd256(values);
    const __m256d upper = _mm512_extractf64x4_pd(values, 1);
    const __m256d sum = _mm256_add_pd(lower, upper);
    const __m128d lower128 = _mm256_castpd256_pd128(sum);
    const __m128d upper128 = _mm256_extractf128_pd(sum, 1);
    const __m128d sum128 = _mm_add_pd(lower128, upper128);
    const __m128d shuffled = _mm_shuffle_pd(sum128, sum128, 1);
    return _mm_cvtsd_f64(_mm_add_pd(sum128, shuffled));
}

static void distributed_avx512_reduce(const void* accumulator, double scalar_x, double scalar_y,
                                      double scalar_z, double* acceleration_x,
                                      double* acceleration_y, double* acceleration_z) {
    const Avx512DistAccumulator* acc = (const Avx512DistAccumulator*)accumulator;
    *acceleration_x = distributed_avx512_horizontal_sum(acc->x) + scalar_x;
    *acceleration_y = distributed_avx512_horizontal_sum(acc->y) + scalar_y;
    *acceleration_z = distributed_avx512_horizontal_sum(acc->z) + scalar_z;
}

static const NBodySimProDistributedSimdOps distributed_avx512_ops = {
    8,
    sizeof(Avx512DistAccumulator),
    distributed_avx512_init,
    distributed_avx512_flush,
    distributed_avx512_reduce,
};

static void evaluate_particle_distributed_avx512(const NBodySimProDistributedSimulation* simulation,
                                                 const NBodySimProBarnesHutTree* tree,
                                                 const NBodySimProParticleSystemView* view,
                                                 const NBodySimProGravity* gravity,
                                                 size_t query_particle, double* acceleration_x,
                                                 double* acceleration_y, double* acceleration_z) {
    Avx512DistAccumulator accumulator;
    distributed_avx512_init(&accumulator);
    n_body_sim_pro_distributed_evaluate_particle_staged(
        simulation, tree, view, gravity, query_particle, &distributed_avx512_ops, &accumulator,
        acceleration_x, acceleration_y, acceleration_z);
}

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    NBodySimProDistributedSimulation* simulation = (NBodySimProDistributedSimulation*)context;
    return n_body_sim_pro_distributed_compute_impl(
        simulation, view, gravity, evaluate_particle_distributed_avx512, error);
}

#else /* !__AVX512F__ */

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_distributed_compute_acceleration(view, gravity, context, error);
}

#endif /* __AVX512F__ */
