#include "n_body_sim_pro/diagnostics/numerics.h"

#include <math.h>

NBodySimProStatus n_body_sim_pro_diagnostics_compute_global(const NBodySimProParticleSystemView* view,
                                               NBodySimProDiagnosticsQuantities* quantities,
                                               NBodySimProError* error) {
    if (view == NULL || quantities == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and quantities must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    const size_t particle_count = view->particle_count;
    const double* const positions_x = view->positions_x;
    const double* const positions_y = view->positions_y;
    const double* const positions_z = view->positions_z;
    const double* const velocities_x = view->velocities_x;
    const double* const velocities_y = view->velocities_y;
    const double* const velocities_z = view->velocities_z;
    const double* const masses = view->masses;

    double kinetic_energy = 0.0;
    double total_mass = 0.0;
    double momentum_x = 0.0;
    double momentum_y = 0.0;
    double momentum_z = 0.0;
    double angular_momentum_x = 0.0;
    double angular_momentum_y = 0.0;
    double angular_momentum_z = 0.0;
    double center_of_mass_numerator_x = 0.0;
    double center_of_mass_numerator_y = 0.0;
    double center_of_mass_numerator_z = 0.0;

    for (size_t i = 0; i < particle_count; ++i) {
        const double mass = masses[i];
        const double velocity_x = velocities_x[i];
        const double velocity_y = velocities_y[i];
        const double velocity_z = velocities_z[i];

        kinetic_energy +=
            0.5 * mass * (velocity_x * velocity_x + velocity_y * velocity_y +
                          velocity_z * velocity_z);
        total_mass += mass;
        momentum_x += mass * velocity_x;
        momentum_y += mass * velocity_y;
        momentum_z += mass * velocity_z;

        const double position_x = positions_x[i];
        const double position_y = positions_y[i];
        const double position_z = positions_z[i];

        angular_momentum_x += mass * (position_y * velocity_z - position_z * velocity_y);
        angular_momentum_y += mass * (position_z * velocity_x - position_x * velocity_z);
        angular_momentum_z += mass * (position_x * velocity_y - position_y * velocity_x);

        center_of_mass_numerator_x += mass * position_x;
        center_of_mass_numerator_y += mass * position_y;
        center_of_mass_numerator_z += mass * position_z;
    }

    quantities->kinetic_energy = kinetic_energy;
    quantities->total_mass = total_mass;
    quantities->total_momentum_x = momentum_x;
    quantities->total_momentum_y = momentum_y;
    quantities->total_momentum_z = momentum_z;
    quantities->total_angular_momentum_x = angular_momentum_x;
    quantities->total_angular_momentum_y = angular_momentum_y;
    quantities->total_angular_momentum_z = angular_momentum_z;
    if (total_mass != 0.0) {
        quantities->center_of_mass_x = center_of_mass_numerator_x / total_mass;
        quantities->center_of_mass_y = center_of_mass_numerator_y / total_mass;
        quantities->center_of_mass_z = center_of_mass_numerator_z / total_mass;
    } else {
        quantities->center_of_mass_x = 0.0;
        quantities->center_of_mass_y = 0.0;
        quantities->center_of_mass_z = 0.0;
    }

    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProStatus n_body_sim_pro_diagnostics_compute_potential_energy(
    const NBodySimProParticleSystemView* view, const NBodySimProGravity* gravity,
    double* potential_energy, NBodySimProError* error) {
    if (view == NULL || gravity == NULL || potential_energy == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view, gravity, and output must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    const size_t particle_count = view->particle_count;
    const double* const positions_x = view->positions_x;
    const double* const positions_y = view->positions_y;
    const double* const positions_z = view->positions_z;
    const double* const masses = view->masses;

    const double gravitational_constant = gravity->gravitational_constant;
    const double softening_squared = gravity->softening_squared;

    double sum = 0.0;
    for (size_t i = 0; i < particle_count; ++i) {
        for (size_t j = i + 1; j < particle_count; ++j) {
            const double delta_x = positions_x[j] - positions_x[i];
            const double delta_y = positions_y[j] - positions_y[i];
            const double delta_z = positions_z[j] - positions_z[i];
            const double distance =
                sqrt(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z +
                     softening_squared);
            sum += masses[i] * masses[j] / distance;
        }
    }

    *potential_energy = -gravitational_constant * sum;
    return N_BODY_SIM_PRO_STATUS_OK;
}
