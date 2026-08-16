#include "n_body_sim_pro/physics/gravity.h"

#include <math.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * NEON SIMD all-pairs gravity kernel (AArch64).
 *
 * AArch64 NEON provides 128-bit vectors, i.e. two doubles per lane, so the
 * inner loop over source particles processes two at a time. All three
 * coordinate deltas and the distance are computed with float64x2_t and the
 * accumulation uses fused multiply-add (vfmaq_f64).
 *
 * Self-interaction: with two particles per lane the query particle occupies
 * lane j or j+1. Its delta is exactly zero, so with zero softening that lane
 * would evaluate 0 * inf = NaN; the lane's distance squared is blended to
 * 1.0 instead, keeping the contribution exactly zero.
 *
 * Numerical note: lane-wise accumulation reorders the sum versus the
 * reference kernel, so results match it within tolerance, not bit-for-bit.
 */

static double horizontal_sum_2(float64x2_t values) {
    return vgetq_lane_f64(values, 0) + vgetq_lane_f64(values, 1);
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

    const float64x2_t position_x_vector = vdupq_n_f64(position_x);
    const float64x2_t position_y_vector = vdupq_n_f64(position_y);
    const float64x2_t position_z_vector = vdupq_n_f64(position_z);
    const float64x2_t constant_vector = vdupq_n_f64(gravitational_constant);
    const float64x2_t softening_vector = vdupq_n_f64(softening_squared);
    const float64x2_t one_vector = vdupq_n_f64(1.0);

    float64x2_t acceleration_x = vdupq_n_f64(0.0);
    float64x2_t acceleration_y = vdupq_n_f64(0.0);
    float64x2_t acceleration_z = vdupq_n_f64(0.0);

    size_t j = 0;
    for (; j + 2 <= particle_count; j += 2) {
        const float64x2_t delta_x =
            vsubq_f64(vld1q_f64(positions_x + j), position_x_vector);
        const float64x2_t delta_y =
            vsubq_f64(vld1q_f64(positions_y + j), position_y_vector);
        const float64x2_t delta_z =
            vsubq_f64(vld1q_f64(positions_z + j), position_z_vector);

        float64x2_t distance_squared =
            vfmaq_f64(vfmaq_f64(vfmaq_f64(softening_vector, delta_x, delta_x),
                                delta_y, delta_y),
                      delta_z, delta_z);

        if (softening_squared == 0.0 && i >= j && i < j + 2) {
            const uint64_t self_bits[2] = {i == j ? ~0ULL : 0ULL,
                                           i == j + 1 ? ~0ULL : 0ULL};
            const uint64x2_t self_mask = vld1q_u64(self_bits);
            distance_squared = vbslq_f64(self_mask, one_vector, distance_squared);
        }

        const float64x2_t distance = vsqrtq_f64(distance_squared);
        const float64x2_t inverse_distance_cubed =
            vdivq_f64(one_vector, vmulq_f64(distance_squared, distance));

        const float64x2_t force_scale = vmulq_f64(
            constant_vector, vmulq_f64(vld1q_f64(masses + j), inverse_distance_cubed));

        acceleration_x = vfmaq_f64(acceleration_x, force_scale, delta_x);
        acceleration_y = vfmaq_f64(acceleration_y, force_scale, delta_y);
        acceleration_z = vfmaq_f64(acceleration_z, force_scale, delta_z);
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

    accelerations_x[i] = horizontal_sum_2(acceleration_x) + scalar_acceleration_x;
    accelerations_y[i] = horizontal_sum_2(acceleration_y) + scalar_acceleration_y;
    accelerations_z[i] = horizontal_sum_2(acceleration_z) + scalar_acceleration_z;
}

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_neon(
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

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_neon(
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

#else /* !__aarch64__ */

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_gravity_compute_acceleration_reference(view, gravity, context, error);
}

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_gravity_compute_acceleration_openmp(view, gravity, context, error);
}

#endif /* __aarch64__ */
