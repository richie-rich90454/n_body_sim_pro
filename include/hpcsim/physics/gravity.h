#ifndef HPCSIM_PHYSICS_GRAVITY_H
#define HPCSIM_PHYSICS_GRAVITY_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"

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

typedef struct HpcsimGravity {
    double gravitational_constant;
    double softening_length;
    double softening_squared;
} HpcsimGravity;

/* Initialize gravity parameters; precomputes softening^2. */
void hpcsim_gravity_init(HpcsimGravity* gravity, double gravitational_constant,
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
HpcsimStatus hpcsim_gravity_compute_acceleration_reference(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error);

/*
 * OpenMP-parallel all-pairs kernel.
 *
 * The outer particle loop is distributed across threads. Each particle's
 * force is still accumulated in the same serial order as the reference, so
 * on a given machine the results are bit-identical to the reference kernel
 * (deterministic parallel reduction). Requires OpenMP support in the build.
 */
HpcsimStatus hpcsim_gravity_compute_acceleration_openmp(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_PHYSICS_GRAVITY_H */
