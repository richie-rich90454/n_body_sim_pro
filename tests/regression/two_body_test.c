#include "hpcsim/diagnostics/numerics.h"
#include "hpcsim/physics/gravity.h"
#include "hpcsim/physics/integrator.h"
#include "test_harness.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Two-body orbital regression test.
 *
 * Two equal masses in a circular orbit about their common center of mass.
 * This is the foundational correctness check for the simulation loop:
 * a stable orbit must remain an orbit, and the Newtonian conservation laws
 * (energy, momentum, center of mass) must hold to the accuracy that a
 * symplectic integrator provides at this timestep.
 *
 * Setup (G = 1, m1 = m2 = 1, separation r = 1):
 *   circular relative speed  v_rel = sqrt(G*(m1+m2)/r) = sqrt(2)
 *   per-body speed about COM = v_rel/2 = sqrt(2)/2
 *   m1 = (-0.5, 0, 0), m2 = (+0.5, 0, 0)
 *   v1 = (0, +sqrt(2)/2, 0), v2 = (0, -sqrt(2)/2, 0)
 *
 * Orbital period T = 2*pi*sqrt(r^3/(G*(m1+m2))) = 2*pi/sqrt(2).
 */

static const double GRAVITATIONAL_CONSTANT = 1.0;
static const double SOFTENING_LENGTH = 0.0;
static const double BODY_MASS = 1.0;
static const double SEPARATION = 1.0;
static const double HALF_SEPARATION = 0.5;
static const double ORBITAL_PERIOD = 2.0 * M_PI / sqrt(2.0);

static double initialize_circular_orbit(HpcsimParticleSystem* particle_system) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 2, &error);

    const double per_body_speed = sqrt(2.0) / 2.0;
    hpcsim_particle_system_set_position(particle_system, 0,
                                        (HpcsimVector3){-HALF_SEPARATION, 0.0, 0.0}, &error);
    hpcsim_particle_system_set_position(particle_system, 1,
                                        (HpcsimVector3){HALF_SEPARATION, 0.0, 0.0}, &error);
    hpcsim_particle_system_set_velocity(particle_system, 0,
                                        (HpcsimVector3){0.0, per_body_speed, 0.0}, &error);
    hpcsim_particle_system_set_velocity(particle_system, 1,
                                        (HpcsimVector3){0.0, -per_body_speed, 0.0}, &error);
    hpcsim_particle_system_set_mass(particle_system, 0, BODY_MASS, &error);
    hpcsim_particle_system_set_mass(particle_system, 1, BODY_MASS, &error);
    return per_body_speed;
}

static double relative_energy_drift_after_orbit(HpcsimIntegratorType integrator,
                                                int orbit_count, int steps_per_orbit,
                                                double* separation_error_out,
                                                double* momentum_magnitude_out,
                                                double* center_of_mass_offset_out) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    if (particle_system == NULL) {
        return -1.0;
    }
    initialize_circular_orbit(particle_system);

    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, GRAVITATIONAL_CONSTANT, SOFTENING_LENGTH);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error);

    const double timestep = ORBITAL_PERIOD / (double)steps_per_orbit;

    HpcsimDiagnosticsQuantities initial_diagnostics;
    double initial_potential_energy = 0.0;
    hpcsim_diagnostics_compute_global(&view, &initial_diagnostics, &error);
    hpcsim_diagnostics_compute_potential_energy(&view, &gravity,
                                                &initial_potential_energy, &error);
    const double initial_total_energy =
        initial_diagnostics.kinetic_energy + initial_potential_energy;

    const int total_steps = orbit_count * steps_per_orbit;
    for (int step = 0; step < total_steps; ++step) {
        hpcsim_integrator_advance(&view, &gravity, integrator, timestep,
                                  hpcsim_gravity_compute_acceleration_reference, &error);
    }

    HpcsimDiagnosticsQuantities final_diagnostics;
    double final_potential_energy = 0.0;
    hpcsim_diagnostics_compute_global(&view, &final_diagnostics, &error);
    hpcsim_diagnostics_compute_potential_energy(&view, &gravity, &final_potential_energy,
                                                &error);
    const double final_total_energy =
        final_diagnostics.kinetic_energy + final_potential_energy;

    HpcsimVector3 position_0;
    HpcsimVector3 position_1;
    hpcsim_particle_system_position(particle_system, 0, &position_0, &error);
    hpcsim_particle_system_position(particle_system, 1, &position_1, &error);
    const double delta_x = position_1.x - position_0.x;
    const double delta_y = position_1.y - position_0.y;
    const double delta_z = position_1.z - position_0.z;
    const double separation = sqrt(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);
    if (separation_error_out != NULL) {
        *separation_error_out = fabs(separation - SEPARATION);
    }

    const double momentum_magnitude =
        sqrt(final_diagnostics.total_momentum_x * final_diagnostics.total_momentum_x +
             final_diagnostics.total_momentum_y * final_diagnostics.total_momentum_y +
             final_diagnostics.total_momentum_z * final_diagnostics.total_momentum_z);
    if (momentum_magnitude_out != NULL) {
        *momentum_magnitude_out = momentum_magnitude;
    }

    const double center_of_mass_offset =
        sqrt(final_diagnostics.center_of_mass_x * final_diagnostics.center_of_mass_x +
             final_diagnostics.center_of_mass_y * final_diagnostics.center_of_mass_y +
             final_diagnostics.center_of_mass_z * final_diagnostics.center_of_mass_z);
    if (center_of_mass_offset_out != NULL) {
        *center_of_mass_offset_out = center_of_mass_offset;
    }

    hpcsim_particle_system_destroy(particle_system);

    const double reference_energy =
        fabs(initial_total_energy) > 0.0 ? fabs(initial_total_energy) : 1.0;
    return fabs(final_total_energy - initial_total_energy) / reference_energy;
}

static void test_leapfrog_keeps_circular_orbit_stable(void) {
    double separation_error = 0.0;
    double momentum_magnitude = 0.0;
    double center_of_mass_offset = 0.0;
    const double energy_drift =
        relative_energy_drift_after_orbit(HPCSIM_INTEGRATOR_LEAPFROG, 8, 500,
                                          &separation_error, &momentum_magnitude,
                                          &center_of_mass_offset);

    HPCSIM_ASSERT(energy_drift >= 0.0);
    HPCSIM_ASSERT(energy_drift < 1e-3);
    HPCSIM_ASSERT(separation_error < 1e-2);
    HPCSIM_ASSERT(momentum_magnitude < 1e-12);
    HPCSIM_ASSERT(center_of_mass_offset < 1e-12);
}

static void test_velocity_verlet_keeps_circular_orbit_stable(void) {
    double separation_error = 0.0;
    double momentum_magnitude = 0.0;
    double center_of_mass_offset = 0.0;
    const double energy_drift =
        relative_energy_drift_after_orbit(HPCSIM_INTEGRATOR_VELOCITY_VERLET, 8, 500,
                                          &separation_error, &momentum_magnitude,
                                          &center_of_mass_offset);

    HPCSIM_ASSERT(energy_drift >= 0.0);
    HPCSIM_ASSERT(energy_drift < 1e-3);
    HPCSIM_ASSERT(separation_error < 1e-2);
    HPCSIM_ASSERT(momentum_magnitude < 1e-12);
    HPCSIM_ASSERT(center_of_mass_offset < 1e-12);
}

static void test_euler_drifts_more_than_symplectic(void) {
    double separation_error = 0.0;
    double momentum_magnitude = 0.0;
    double center_of_mass_offset = 0.0;
    const double euler_drift =
        relative_energy_drift_after_orbit(HPCSIM_INTEGRATOR_EULER, 1, 500,
                                          &separation_error, &momentum_magnitude,
                                          &center_of_mass_offset);

    const double verlet_drift =
        relative_energy_drift_after_orbit(HPCSIM_INTEGRATOR_VELOCITY_VERLET, 1, 500,
                                          NULL, NULL, NULL);

    HPCSIM_ASSERT(euler_drift > 1e-3);
    HPCSIM_ASSERT(euler_drift > verlet_drift);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_leapfrog_keeps_circular_orbit_stable);
    HPCSIM_TEST_RUN(test_velocity_verlet_keeps_circular_orbit_stable);
    HPCSIM_TEST_RUN(test_euler_drifts_more_than_symplectic);
    return HPCSIM_TEST_SUITE_END();
}
