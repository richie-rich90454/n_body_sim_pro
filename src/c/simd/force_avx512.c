#include "n_body_sim_pro/physics/gravity.h"

#include <math.h>

#if defined(__AVX512F__)
#include <immintrin.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * AVX-512 SIMD all-pairs gravity kernel.
 *
 * A direct widening of the AVX2 kernel to 512-bit lanes: eight source
 * particles are processed at a time, all three coordinate deltas and the
 * distance are computed in 512-bit lanes, and the acceleration accumulation
 * uses fused multiply-add. The kernel must be compiled with -mavx512f -mfma.
 *
 * Self-interaction: a particle's own lane contributes zero force because its
 * delta is exactly zero. With zero softening that lane would evaluate
 * 0 * inf = NaN, so when softening is zero the self lane's distance squared
 * is blended to 1.0, keeping the contribution exactly zero.
 *
 * Numerical note: lane-wise accumulation reorders the sum versus the
 * reference kernel, so results match it within tolerance, not bit-for-bit.
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

static void compute_acceleration_block(const NBodySimProParticleSystemView* view,
                                       const NBodySimProGravity* gravity, size_t i) {
    const size_t particle_count = view->particle_count;
    const double* const positions_x = view->positions_x;
    const double* const positions_y = view->positions_y;
    const double* const positions_z = view->positions_z;
    const double* const masses = view->masses;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    const double position_x = positions_x[i];
    const double position_y = positions_y[i];
    const double position_z = positions_z[i];
    const double gravitational_constant = gravity->gravitational_constant;
    const double softening_squared = gravity->softening_squared;

    const __m512d position_x_vector = _mm512_set1_pd(position_x);
    const __m512d position_y_vector = _mm512_set1_pd(position_y);
    const __m512d position_z_vector = _mm512_set1_pd(position_z);
    const __m512d constant_vector = _mm512_set1_pd(gravitational_constant);
    const __m512d softening_vector = _mm512_set1_pd(softening_squared);
    const __m512d one_vector = _mm512_set1_pd(1.0);

    __m512d acceleration_x = _mm512_setzero_pd();
    __m512d acceleration_y = _mm512_setzero_pd();
    __m512d acceleration_z = _mm512_setzero_pd();

    size_t j = 0;
    for (; j + 8 <= particle_count; j += 8) {
        const __m512d delta_x =
            _mm512_sub_pd(_mm512_loadu_pd(positions_x + j), position_x_vector);
        const __m512d delta_y =
            _mm512_sub_pd(_mm512_loadu_pd(positions_y + j), position_y_vector);
        const __m512d delta_z =
            _mm512_sub_pd(_mm512_loadu_pd(positions_z + j), position_z_vector);

        __m512d distance_squared =
            _mm512_fmadd_pd(delta_x, delta_x,
                            _mm512_fmadd_pd(delta_y, delta_y,
                                            _mm512_fmadd_pd(delta_z, delta_z,
                                                            softening_vector)));

        if (softening_squared == 0.0 && i >= j && i < j + 8) {
            __mmask8 self_mask = 0;
            for (int lane = 0; lane < 8; ++lane) {
                if (i == j + (size_t)lane) {
                    self_mask |= (__mmask8)(1u << lane);
                }
            }
            distance_squared =
                _mm512_mask_blend_pd(self_mask, distance_squared, one_vector);
        }

        const __m512d distance = _mm512_sqrt_pd(distance_squared);
        const __m512d inverse_distance_cubed =
            _mm512_div_pd(one_vector, _mm512_mul_pd(distance_squared, distance));

        const __m512d force_scale = _mm512_mul_pd(
            constant_vector, _mm512_mul_pd(_mm512_loadu_pd(masses + j),
                                           inverse_distance_cubed));

        acceleration_x = _mm512_fmadd_pd(force_scale, delta_x, acceleration_x);
        acceleration_y = _mm512_fmadd_pd(force_scale, delta_y, acceleration_y);
        acceleration_z = _mm512_fmadd_pd(force_scale, delta_z, acceleration_z);
    }

    double scalar_acceleration_x = 0.0;
    double scalar_acceleration_y = 0.0;
    double scalar_acceleration_z = 0.0;
    for (; j < particle_count; ++j) {
        if (j == i) {
            continue;
        }
        const double delta_x = positions_x[j] - position_x;
        const double delta_y = positions_y[j] - position_y;
        const double delta_z = positions_z[j] - position_z;
        const double distance_squared =
            delta_x * delta_x + delta_y * delta_y + delta_z * delta_z + softening_squared;
        const double inverse_distance_cubed =
            1.0 / (distance_squared * sqrt(distance_squared));
        const double force_scale =
            gravitational_constant * masses[j] * inverse_distance_cubed;
        scalar_acceleration_x += force_scale * delta_x;
        scalar_acceleration_y += force_scale * delta_y;
        scalar_acceleration_z += force_scale * delta_z;
    }

    accelerations_x[i] =
        horizontal_sum_8(acceleration_x) + scalar_acceleration_x;
    accelerations_y[i] =
        horizontal_sum_8(acceleration_y) + scalar_acceleration_y;
    accelerations_z[i] =
        horizontal_sum_8(acceleration_z) + scalar_acceleration_z;
}

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    (void)context;
    if (view == NULL || gravity == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and gravity parameters must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < view->particle_count; ++i) {
        compute_acceleration_block(view, gravity, i);
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    (void)context;
    if (view == NULL || gravity == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and gravity parameters must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long i = 0; i < (long long)view->particle_count; ++i) {
        compute_acceleration_block(view, gravity, (size_t)i);
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

#else /* !__AVX512F__ */

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_gravity_compute_acceleration_reference(view, gravity, context, error);
}

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_gravity_compute_acceleration_openmp(view, gravity, context, error);
}

#endif /* __AVX512F__ */
