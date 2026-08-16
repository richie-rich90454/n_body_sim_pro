#include "n_body_sim_pro/mpi/distributed_barnes_hut.h"

#include "distributed_barnes_hut_internal.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>

/*
 * NEON SIMD acceleration of the distributed Barnes-Hut traversal (AArch64).
 *
 * Two staged interactions per 128-bit lane, applied with fused multiply-add.
 * The essential-tree exchange and the local tree build are byte-identical
 * to the scalar kernel; only the per-particle force accumulation is
 * vectorized, so the result matches the scalar distributed kernel within
 * floating-point tolerance.
 */

typedef struct NeonDistAccumulator {
    float64x2_t x;
    float64x2_t y;
    float64x2_t z;
} NeonDistAccumulator;

static void distributed_neon_init(void* accumulator) {
    NeonDistAccumulator* acc = (NeonDistAccumulator*)accumulator;
    acc->x = vdupq_n_f64(0.0);
    acc->y = vdupq_n_f64(0.0);
    acc->z = vdupq_n_f64(0.0);
}

static void distributed_neon_flush(void* accumulator, const double* dx, const double* dy,
                                   const double* dz, const double* mass, double softening_squared,
                                   double gravitational_constant) {
    NeonDistAccumulator* acc = (NeonDistAccumulator*)accumulator;
    const float64x2_t delta_x = vld1q_f64(dx);
    const float64x2_t delta_y = vld1q_f64(dy);
    const float64x2_t delta_z = vld1q_f64(dz);
    const float64x2_t mass_vector = vld1q_f64(mass);

    const float64x2_t distance_squared =
        vfmaq_f64(vfmaq_f64(vfmaq_f64(vdupq_n_f64(softening_squared), delta_x, delta_x),
                            delta_y, delta_y),
                  delta_z, delta_z);
    const float64x2_t distance = vsqrtq_f64(distance_squared);
    const float64x2_t inverse_distance_cubed =
        vdivq_f64(vdupq_n_f64(1.0), vmulq_f64(distance_squared, distance));
    const float64x2_t force_scale =
        vmulq_f64(vdupq_n_f64(gravitational_constant),
                  vmulq_f64(mass_vector, inverse_distance_cubed));

    acc->x = vfmaq_f64(acc->x, force_scale, delta_x);
    acc->y = vfmaq_f64(acc->y, force_scale, delta_y);
    acc->z = vfmaq_f64(acc->z, force_scale, delta_z);
}

static double distributed_neon_horizontal_sum(float64x2_t values) {
    return vgetq_lane_f64(values, 0) + vgetq_lane_f64(values, 1);
}

static void distributed_neon_reduce(const void* accumulator, double scalar_x, double scalar_y,
                                    double scalar_z, double* acceleration_x,
                                    double* acceleration_y, double* acceleration_z) {
    const NeonDistAccumulator* acc = (const NeonDistAccumulator*)accumulator;
    *acceleration_x = distributed_neon_horizontal_sum(acc->x) + scalar_x;
    *acceleration_y = distributed_neon_horizontal_sum(acc->y) + scalar_y;
    *acceleration_z = distributed_neon_horizontal_sum(acc->z) + scalar_z;
}

static const NBodySimProDistributedSimdOps distributed_neon_ops = {
    2,
    sizeof(NeonDistAccumulator),
    distributed_neon_init,
    distributed_neon_flush,
    distributed_neon_reduce,
};

static void evaluate_particle_distributed_neon(const NBodySimProDistributedSimulation* simulation,
                                               const NBodySimProBarnesHutTree* tree,
                                               const NBodySimProParticleSystemView* view,
                                               const NBodySimProGravity* gravity,
                                               size_t query_particle, double* acceleration_x,
                                               double* acceleration_y, double* acceleration_z) {
    NeonDistAccumulator accumulator;
    distributed_neon_init(&accumulator);
    n_body_sim_pro_distributed_evaluate_particle_staged(
        simulation, tree, view, gravity, query_particle, &distributed_neon_ops, &accumulator,
        acceleration_x, acceleration_y, acceleration_z);
}

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    NBodySimProDistributedSimulation* simulation = (NBodySimProDistributedSimulation*)context;
    return n_body_sim_pro_distributed_compute_impl(
        simulation, view, gravity, evaluate_particle_distributed_neon, error);
}

#else /* !__aarch64__ */

NBodySimProStatus n_body_sim_pro_distributed_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error) {
    return n_body_sim_pro_distributed_compute_acceleration(view, gravity, context, error);
}

#endif /* __aarch64__ */
