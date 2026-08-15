#include "hpcsim/physics/gravity.h"

#include <math.h>

#if defined(__AVX2__)
#include <immintrin.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * AVX2 SIMD all-pairs gravity kernel.
 *
 * The inner loop over source particles is vectorized: four source particles
 * at a time, all three coordinate deltas and the distance are computed in
 * 256-bit lanes, and the acceleration accumulation uses fused multiply-add
 * when FMA is available (guarded by __FMA__). The kernel must be compiled
 * with -mavx2 -mfma.
 *
 * Self-interaction: a particle's own lane contributes zero force because its
 * delta is exactly zero. With zero softening that lane would evaluate
 * 0 * inf = NaN, so when softening is zero the self lane's distance squared
 * is blended to 1.0, keeping the contribution exactly zero.
 *
 * Numerical note: lane-wise accumulation reorders the sum versus the
 * reference kernel, so results match it within tolerance, not bit-for-bit.
 */

static double horizontal_sum_4(__m256d values) {
    __m128d lower = _mm256_castpd256_pd128(values);
    __m128d upper = _mm256_extractf128_pd(values, 1);
    __m128d sum = _mm_add_pd(lower, upper);
    sum = _mm_hadd_pd(sum, sum);
    return _mm_cvtsd_f64(sum);
}

static void compute_acceleration_block(const HpcsimParticleSystemView* view,
                                       const HpcsimGravity* gravity, size_t i) {
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

    const __m256d position_x_vector = _mm256_set1_pd(position_x);
    const __m256d position_y_vector = _mm256_set1_pd(position_y);
    const __m256d position_z_vector = _mm256_set1_pd(position_z);
    const __m256d constant_vector = _mm256_set1_pd(gravitational_constant);
    const __m256d softening_vector = _mm256_set1_pd(softening_squared);
    const __m256d one_vector = _mm256_set1_pd(1.0);

    __m256d acceleration_x = _mm256_setzero_pd();
    __m256d acceleration_y = _mm256_setzero_pd();
    __m256d acceleration_z = _mm256_setzero_pd();

    size_t j = 0;
    for (; j + 4 <= particle_count; j += 4) {
        const __m256d delta_x =
            _mm256_sub_pd(_mm256_loadu_pd(positions_x + j), position_x_vector);
        const __m256d delta_y =
            _mm256_sub_pd(_mm256_loadu_pd(positions_y + j), position_y_vector);
        const __m256d delta_z =
            _mm256_sub_pd(_mm256_loadu_pd(positions_z + j), position_z_vector);

        __m256d distance_squared =
            _mm256_fmadd_pd(delta_x, delta_x,
                            _mm256_fmadd_pd(delta_y, delta_y,
                                            _mm256_fmadd_pd(delta_z, delta_z,
                                                            softening_vector)));

        if (softening_squared == 0.0 && i >= j && i < j + 4) {
            /* blendv selects on the sign bit: -1 picks the guard value. */
            const __m256d self_mask = _mm256_castsi256_pd(_mm256_setr_epi64x(
                i == j ? -1 : 0, i == j + 1 ? -1 : 0, i == j + 2 ? -1 : 0,
                i == j + 3 ? -1 : 0));
            distance_squared = _mm256_blendv_pd(distance_squared, one_vector, self_mask);
        }

        const __m256d distance = _mm256_sqrt_pd(distance_squared);
        const __m256d inverse_distance_cubed =
            _mm256_div_pd(one_vector, _mm256_mul_pd(distance_squared, distance));

        const __m256d force_scale = _mm256_mul_pd(
            constant_vector, _mm256_mul_pd(_mm256_loadu_pd(masses + j),
                                           inverse_distance_cubed));

        acceleration_x = _mm256_fmadd_pd(force_scale, delta_x, acceleration_x);
        acceleration_y = _mm256_fmadd_pd(force_scale, delta_y, acceleration_y);
        acceleration_z = _mm256_fmadd_pd(force_scale, delta_z, acceleration_z);
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
        horizontal_sum_4(acceleration_x) + scalar_acceleration_x;
    accelerations_y[i] =
        horizontal_sum_4(acceleration_y) + scalar_acceleration_y;
    accelerations_z[i] =
        horizontal_sum_4(acceleration_z) + scalar_acceleration_z;
}

HpcsimStatus hpcsim_gravity_compute_acceleration_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    if (view == NULL || gravity == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and gravity parameters must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < view->particle_count; ++i) {
        compute_acceleration_block(view, gravity, i);
    }
    return HPCSIM_STATUS_OK;
}

HpcsimStatus hpcsim_gravity_compute_acceleration_openmp_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    if (view == NULL || gravity == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and gravity parameters must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (long long i = 0; i < (long long)view->particle_count; ++i) {
        compute_acceleration_block(view, gravity, (size_t)i);
    }
    return HPCSIM_STATUS_OK;
}

#else /* !__AVX2__ */

HpcsimStatus hpcsim_gravity_compute_acceleration_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    return hpcsim_gravity_compute_acceleration_reference(view, gravity, error);
}

HpcsimStatus hpcsim_gravity_compute_acceleration_openmp_avx2(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    return hpcsim_gravity_compute_acceleration_openmp(view, gravity, error);
}

#endif /* __AVX2__ */
