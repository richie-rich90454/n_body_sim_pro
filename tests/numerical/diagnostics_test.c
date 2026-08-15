#include "hpcsim/diagnostics/numerics.h"
#include "test_harness.h"

#include <math.h>

/*
 * Numerical tests for the conservation diagnostics.
 *
 * Uses a two-particle state whose kinetic energy, momentum, angular
 * momentum, and center of mass are known exactly by hand.
 */

static void test_global_quantities_analytic(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 2, &error);

    /* m1 = 2 at (1,2,3) with v1 = (1,0,0); m2 = 3 at (4,5,6) with v2 = (0,1,0). */
    hpcsim_particle_system_set_position(particle_system, 0, (HpcsimVector3){1, 2, 3}, &error);
    hpcsim_particle_system_set_position(particle_system, 1, (HpcsimVector3){4, 5, 6}, &error);
    hpcsim_particle_system_set_velocity(particle_system, 0, (HpcsimVector3){1, 0, 0}, &error);
    hpcsim_particle_system_set_velocity(particle_system, 1, (HpcsimVector3){0, 1, 0}, &error);
    hpcsim_particle_system_set_mass(particle_system, 0, 2.0, &error);
    hpcsim_particle_system_set_mass(particle_system, 1, 3.0, &error);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimDiagnosticsQuantities quantities;
    HPCSIM_ASSERT(hpcsim_diagnostics_compute_global(&view, &quantities, &error) ==
                  HPCSIM_STATUS_OK);

    HPCSIM_ASSERT_NEAR(quantities.kinetic_energy, 2.5, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_mass, 5.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_momentum_x, 2.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_momentum_y, 3.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_momentum_z, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_angular_momentum_x, -18.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_angular_momentum_y, 6.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.total_angular_momentum_z, 8.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.center_of_mass_x, 14.0 / 5.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.center_of_mass_y, 19.0 / 5.0, 1e-15);
    HPCSIM_ASSERT_NEAR(quantities.center_of_mass_z, 24.0 / 5.0, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_potential_energy_single_pair(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 2, &error);
    hpcsim_particle_system_set_position(particle_system, 0, (HpcsimVector3){-0.5, 0, 0}, &error);
    hpcsim_particle_system_set_position(particle_system, 1, (HpcsimVector3){0.5, 0, 0}, &error);
    hpcsim_particle_system_set_mass(particle_system, 0, 1.0, &error);
    hpcsim_particle_system_set_mass(particle_system, 1, 1.0, &error);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.0);
    double potential_energy = 0.0;
    HPCSIM_ASSERT(hpcsim_diagnostics_compute_potential_energy(
                      &view, &gravity, &potential_energy, &error) == HPCSIM_STATUS_OK);

    /* U = -G*m1*m2/r = -1 for unit masses separated by 1. */
    HPCSIM_ASSERT_NEAR(potential_energy, -1.0, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_empty_system_quantities_are_zero(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(4);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimDiagnosticsQuantities quantities;
    HPCSIM_ASSERT(hpcsim_diagnostics_compute_global(&view, &quantities, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_NEAR(quantities.kinetic_energy, 0.0, 0.0);
    HPCSIM_ASSERT_NEAR(quantities.total_mass, 0.0, 0.0);
    HPCSIM_ASSERT_NEAR(quantities.total_momentum_x, 0.0, 0.0);
    HPCSIM_ASSERT_NEAR(quantities.total_angular_momentum_z, 0.0, 0.0);
    HPCSIM_ASSERT_NEAR(quantities.center_of_mass_x, 0.0, 0.0);

    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_global_quantities_analytic);
    HPCSIM_TEST_RUN(test_potential_energy_single_pair);
    HPCSIM_TEST_RUN(test_empty_system_quantities_are_zero);
    return HPCSIM_TEST_SUITE_END();
}
