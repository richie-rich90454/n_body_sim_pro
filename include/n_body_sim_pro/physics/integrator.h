#ifndef N_BODY_SIM_PRO_PHYSICS_INTEGRATOR_H
#define N_BODY_SIM_PRO_PHYSICS_INTEGRATOR_H

#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"
#include "n_body_sim_pro/physics/gravity.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Time integrators for the gravitational N-body system.
 *
 * Numerical properties (documented fully in docs/physics/integrators.md):
 *
 *   Euler            : first-order, non-symplectic. Energy drifts secularly;
 *                      useful only as a teaching/baseline method.
 *   Leapfrog         : second-order, symplectic (kick-drift-kick form).
 *                      Long-term energy error is bounded for fixed timesteps.
 *                      The astrophysical default.
 *   Velocity Verlet  : second-order, symplectic. For constant timestep it is
 *                      algebraically identical to leapfrog; both are kept as
 *                      distinct implementations for clarity.
 *
 * All integrators operate on the particle system's current accelerations:
 * callers must have computed accelerations for the current state before the
 * first step. Each symplectic step recomputes accelerations once mid-step.
 */

typedef enum NBodySimProIntegratorType {
    N_BODY_SIM_PRO_INTEGRATOR_EULER,
    N_BODY_SIM_PRO_INTEGRATOR_LEAPFROG,
    N_BODY_SIM_PRO_INTEGRATOR_VELOCITY_VERLET
} NBodySimProIntegratorType;

/* Human-readable integrator name. Never returns NULL. */
const char* n_body_sim_pro_integrator_type_string(NBodySimProIntegratorType integrator);

/*
 * A force kernel: computes accelerations for every particle into the view's
 * acceleration arrays. Used by the integrators so they stay decoupled from
 * any specific algorithm (reference all-pairs, OpenMP, SIMD, Barnes-Hut).
 * `context` carries algorithm-specific state (e.g. a Barnes-Hut tree) and
 * may be NULL for kernels that need none.
 */
typedef NBodySimProStatus (*NBodySimProForceFunction)(const NBodySimProParticleSystemView* view,
                                            const NBodySimProGravity* gravity,
                                            void* context, NBodySimProError* error);

/*
 * Advance the system by one timestep.
 *
 * `view` is read/written in place. `force_function` is invoked once (Euler)
 * or twice (leapfrog, velocity Verlet) during the step, each time with
 * `force_context`. The initial accelerations must be valid before the first
 * call for symplectic methods.
 */
NBodySimProStatus n_body_sim_pro_integrator_advance(NBodySimProParticleSystemView* view,
                                       const NBodySimProGravity* gravity,
                                       NBodySimProIntegratorType integrator,
                                       double timestep,
                                       NBodySimProForceFunction force_function,
                                       void* force_context, NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_PHYSICS_INTEGRATOR_H */
