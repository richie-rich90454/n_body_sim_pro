#ifndef N_BODY_SIM_PRO_DIAGNOSTICS_NUMERICS_H
#define N_BODY_SIM_PRO_DIAGNOSTICS_NUMERICS_H

#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"
#include "n_body_sim_pro/physics/gravity.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Global conservation diagnostics.
 *
 * All quantities are computed by direct summation over particles. Kinetic
 * energy, momentum, angular momentum, and center of mass are O(N). Potential
 * energy is O(N^2) because it sums every interacting pair, so it is exposed
 * as a separate function and should only be called for systems where that
 * cost is acceptable (or computed approximately by a force tree).
 */

typedef struct NBodySimProDiagnosticsQuantities {
    double kinetic_energy;
    double total_mass;
    double total_momentum_x;
    double total_momentum_y;
    double total_momentum_z;
    double total_angular_momentum_x;
    double total_angular_momentum_y;
    double total_angular_momentum_z;
    double center_of_mass_x;
    double center_of_mass_y;
    double center_of_mass_z;
} NBodySimProDiagnosticsQuantities;

/*
 * Compute the O(N) global quantities for the current state.
 *
 *   kinetic energy      : 0.5 * sum_i m_i |v_i|^2
 *   total momentum      : sum_i m_i v_i
 *   angular momentum    : sum_i m_i (r_i x v_i)    (about the origin)
 *   center of mass      : sum_i m_i r_i / sum_i m_i
 */
NBodySimProStatus n_body_sim_pro_diagnostics_compute_global(const NBodySimProParticleSystemView* view,
                                               NBodySimProDiagnosticsQuantities* quantities,
                                               NBodySimProError* error);

/*
 * Compute the O(N^2) reference potential energy
 *
 *   U = -G * sum_{i<j} m_i m_j / sqrt(|r_j - r_i|^2 + eps^2)
 *
 * using the same softened pair potential the forces derive from. Intended for
 * small systems and correctness checks; do not call in the simulation loop
 * for large N.
 */
NBodySimProStatus n_body_sim_pro_diagnostics_compute_potential_energy(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity,
    double* potential_energy, NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_DIAGNOSTICS_NUMERICS_H */
