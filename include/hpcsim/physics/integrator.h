#ifndef HPCSIM_PHYSICS_INTEGRATOR_H
#define HPCSIM_PHYSICS_INTEGRATOR_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"
#include "hpcsim/physics/gravity.h"

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

typedef enum HpcsimIntegratorType {
    HPCSIM_INTEGRATOR_EULER,
    HPCSIM_INTEGRATOR_LEAPFROG,
    HPCSIM_INTEGRATOR_VELOCITY_VERLET
} HpcsimIntegratorType;

/* Human-readable integrator name. Never returns NULL. */
const char* hpcsim_integrator_type_string(HpcsimIntegratorType integrator);

/*
 * A force kernel: computes accelerations for every particle into the view's
 * acceleration arrays. Used by the integrators so they stay decoupled from
 * any specific algorithm (reference all-pairs, Barnes-Hut, ...).
 */
typedef HpcsimStatus (*HpcsimForceFunction)(const HpcsimParticleSystemView* view,
                                            const HpcsimGravity* gravity,
                                            HpcsimError* error);

/*
 * Advance the system by one timestep.
 *
 * `view` is read/written in place. `force_function` is invoked once (Euler)
 * or twice (leapfrog, velocity Verlet) during the step. The initial
 * accelerations must be valid before the first call for symplectic methods.
 */
HpcsimStatus hpcsim_integrator_advance(HpcsimParticleSystemView* view,
                                       const HpcsimGravity* gravity,
                                       HpcsimIntegratorType integrator,
                                       double timestep,
                                       HpcsimForceFunction force_function,
                                       HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_PHYSICS_INTEGRATOR_H */
