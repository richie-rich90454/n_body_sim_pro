#ifndef N_BODY_SIM_PRO_PHYSICS_GRAVITY_H
#define N_BODY_SIM_PRO_PHYSICS_GRAVITY_H

#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Newtonian gravity with Plummer softening.
 *
 * The acceleration on particle i from particle j is:
 *
 *   a_i = G * m_j * (r_j - r_i) / (|r_j - r_i|^2 + eps^2)^(3/2)
 *
 * where G is the gravitational constant and eps is the softening length.
 * Softening caps the force at small separations, which prevents the
 * 1/r^2 singularity from ejecting particles to infinity. It also improves
 * energy conservation for collisionless systems. The softening parameter is
 * documented in docs/physics/gravity.md.
 */

typedef struct NBodySimProGravity {
    double gravitational_constant;
    double softening_length;
    double softening_squared;
} NBodySimProGravity;

/* Initialize gravity parameters; precomputes softening^2. */
void n_body_sim_pro_gravity_init(NBodySimProGravity* gravity, double gravitational_constant,
                         double softening_length);

/*
 * Reference O(N^2) all-pairs acceleration kernel, scalar single-threaded
 * double precision.
 *
 * This is the deliberately simple correctness reference. Every optimized
 * kernel (OpenMP, SIMD, Barnes-Hut) is validated against it. It is
 * intentionally kept slow and readable; never delete it.
 *
 * The accelerations of all particles are computed into the view's
 * acceleration arrays (not accumulated).
 */
NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_reference(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

/*
 * OpenMP-parallel all-pairs kernel.
 *
 * The outer particle loop is distributed across threads. Each particle's
 * force is still accumulated in the same serial order as the reference, so
 * on a given machine the results are bit-identical to the reference kernel
 * (deterministic parallel reduction). Requires OpenMP support in the build.
 */
NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

/*
 * AVX2 SIMD all-pairs kernels.
 *
 * The inner force loop is vectorized across source particles with 256-bit
 * FMA. Accumulation order differs from the reference, so results agree with
 * it within floating-point tolerance rather than bit-for-bit. The
 * `_openmp_avx2` variant additionally distributes the outer loop across
 * OpenMP threads.
 *
 * On CPUs without AVX2+FMA these functions degrade to the reference kernel,
 * so their symbols always exist.
 */
NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_avx2(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_avx2(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

/*
 * AVX-512 SIMD all-pairs kernels.
 *
 * Identical to the AVX2 variants but with 512-bit lanes: eight source
 * particles are processed at a time. On CPUs without AVX-512 these functions
 * degrade to the reference kernel, so their symbols always exist.
 */
NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_avx512(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

/*
 * NEON SIMD all-pairs kernels (AArch64).
 *
 * Two source particles per 128-bit lane, computed with fused multiply-add.
 * On non-ARM targets these functions degrade to the reference kernel, so
 * their symbols always exist.
 */
NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

NBodySimProStatus n_body_sim_pro_gravity_compute_acceleration_openmp_neon(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity, void* context,
    NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_PHYSICS_GRAVITY_H */
