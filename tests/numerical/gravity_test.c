#include "hpcsim/physics/gravity.h"
#include "test_harness.h"

#include <math.h>

/*
 * Numerical tests for the reference all-pairs gravity kernel.
 *
 * The reference kernel is validated against the analytic two-body
 * acceleration and against the Newtonian symmetry a_ij = -a_ji.
 */

static HpcsimParticleSystem* make_two_particle_system(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    if (particle_system == NULL) {
        return NULL;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 2, &error);
    hpcsim_particle_system_set_position(particle_system, 0, (HpcsimVector3){-0.5, 0.0, 0.0},
                                        &error);
    hpcsim_particle_system_set_position(particle_system, 1, (HpcsimVector3){0.5, 0.0, 0.0},
                                        &error);
    hpcsim_particle_system_set_mass(particle_system, 0, 1.0, &error);
    hpcsim_particle_system_set_mass(particle_system, 1, 1.0, &error);
    return particle_system;
}

static void test_analytic_two_body_acceleration(void) {
    HpcsimParticleSystem* particle_system = make_two_particle_system();
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.0);
    HPCSIM_ASSERT(hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimVector3 acceleration_0;
    HpcsimVector3 acceleration_1;
    hpcsim_particle_system_acceleration(particle_system, 0, &acceleration_0, &error);
    hpcsim_particle_system_acceleration(particle_system, 1, &acceleration_1, &error);

    /* Analytic: a_0 = +G*m1*(r1-r0)/r^3 = (1, 0, 0), a_1 = (-1, 0, 0). */
    HPCSIM_ASSERT_NEAR(acceleration_0.x, 1.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_0.y, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_0.z, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_1.x, -1.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_1.y, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_1.z, 0.0, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_self_force_is_excluded(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(1);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 1, &error);
    hpcsim_particle_system_set_position(particle_system, 0, (HpcsimVector3){1.0, 1.0, 1.0},
                                        &error);
    hpcsim_particle_system_set_mass(particle_system, 0, 1000.0, &error);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);
    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.0);
    HPCSIM_ASSERT(hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimVector3 acceleration;
    hpcsim_particle_system_acceleration(particle_system, 0, &acceleration, &error);
    HPCSIM_ASSERT_NEAR(acceleration.x, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration.y, 0.0, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration.z, 0.0, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_softening_bounds_force(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_set_particle_count(particle_system, 2, &error);
    hpcsim_particle_system_set_position(particle_system, 0, (HpcsimVector3){0.0, 0.0, 0.0},
                                        &error);
    hpcsim_particle_system_set_position(particle_system, 1, (HpcsimVector3){1e-8, 0.0, 0.0},
                                        &error);
    hpcsim_particle_system_set_mass(particle_system, 0, 1.0, &error);
    hpcsim_particle_system_set_mass(particle_system, 1, 1.0, &error);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity softened;
    hpcsim_gravity_init(&softened, 1.0, 1e-2);
    HPCSIM_ASSERT(hpcsim_gravity_compute_acceleration_reference(&view, &softened, &error) ==
                  HPCSIM_STATUS_OK);
    HpcsimVector3 acceleration;
    hpcsim_particle_system_acceleration(particle_system, 1, &acceleration, &error);

    /* With eps = 1e-2 the softened force magnitude is bounded well below
     * the unsoftened 1/r^2 = 1e16, and particle 1 is pulled toward the
     * origin (negative x). */
    HPCSIM_ASSERT(fabs(acceleration.x) < 1.0e6);
    HPCSIM_ASSERT(acceleration.x < 0.0);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_mutual_force_antisymmetry(void) {
    HpcsimParticleSystem* particle_system = make_two_particle_system();
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);
    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.3);
    HPCSIM_ASSERT(hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimVector3 acceleration_0;
    HpcsimVector3 acceleration_1;
    hpcsim_particle_system_acceleration(particle_system, 0, &acceleration_0, &error);
    hpcsim_particle_system_acceleration(particle_system, 1, &acceleration_1, &error);

    /* Equal masses: m_i a_i must be equal and opposite. */
    HPCSIM_ASSERT_NEAR(acceleration_0.x, -acceleration_1.x, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_0.y, -acceleration_1.y, 1e-15);
    HPCSIM_ASSERT_NEAR(acceleration_0.z, -acceleration_1.z, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_analytic_two_body_acceleration);
    HPCSIM_TEST_RUN(test_self_force_is_excluded);
    HPCSIM_TEST_RUN(test_softening_bounds_force);
    HPCSIM_TEST_RUN(test_mutual_force_antisymmetry);
    return HPCSIM_TEST_SUITE_END();
}
