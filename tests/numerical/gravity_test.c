#include "n_body_sim_pro/physics/gravity.h"
#include "test_harness.h"

#include <math.h>

/*
 * Numerical tests for the reference all-pairs gravity kernel.
 *
 * The reference kernel is validated against the analytic two-body
 * acceleration and against the Newtonian symmetry a_ij = -a_ji.
 */

static NBodySimProParticleSystem* make_two_particle_system(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(2);
    if (particle_system == NULL) {
        return NULL;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    n_body_sim_pro_particle_system_set_particle_count(particle_system, 2, &error);
    n_body_sim_pro_particle_system_set_position(particle_system, 0, (NBodySimProVector3){-0.5, 0.0, 0.0},
                                        &error);
    n_body_sim_pro_particle_system_set_position(particle_system, 1, (NBodySimProVector3){0.5, 0.0, 0.0},
                                        &error);
    n_body_sim_pro_particle_system_set_mass(particle_system, 0, 1.0, &error);
    n_body_sim_pro_particle_system_set_mass(particle_system, 1, 1.0, &error);
    return particle_system;
}

static void test_analytic_two_body_acceleration(void) {
    NBodySimProParticleSystem* particle_system = make_two_particle_system();
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.0);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProVector3 acceleration_0;
    NBodySimProVector3 acceleration_1;
    n_body_sim_pro_particle_system_acceleration(particle_system, 0, &acceleration_0, &error);
    n_body_sim_pro_particle_system_acceleration(particle_system, 1, &acceleration_1, &error);

    /* Analytic: a_0 = +G*m1*(r1-r0)/r^3 = (1, 0, 0), a_1 = (-1, 0, 0). */
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.x, 1.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.y, 0.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.z, 0.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_1.x, -1.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_1.y, 0.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_1.z, 0.0, 1e-15);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_self_force_is_excluded(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(1);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    n_body_sim_pro_particle_system_set_particle_count(particle_system, 1, &error);
    n_body_sim_pro_particle_system_set_position(particle_system, 0, (NBodySimProVector3){1.0, 1.0, 1.0},
                                        &error);
    n_body_sim_pro_particle_system_set_mass(particle_system, 0, 1000.0, &error);

    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);
    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.0);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProVector3 acceleration;
    n_body_sim_pro_particle_system_acceleration(particle_system, 0, &acceleration, &error);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration.x, 0.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration.y, 0.0, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration.z, 0.0, 1e-15);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_softening_bounds_force(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(2);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    n_body_sim_pro_particle_system_set_particle_count(particle_system, 2, &error);
    n_body_sim_pro_particle_system_set_position(particle_system, 0, (NBodySimProVector3){0.0, 0.0, 0.0},
                                        &error);
    n_body_sim_pro_particle_system_set_position(particle_system, 1, (NBodySimProVector3){1e-8, 0.0, 0.0},
                                        &error);
    n_body_sim_pro_particle_system_set_mass(particle_system, 0, 1.0, &error);
    n_body_sim_pro_particle_system_set_mass(particle_system, 1, 1.0, &error);

    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    NBodySimProGravity softened;
    n_body_sim_pro_gravity_init(&softened, 1.0, 1e-2);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_gravity_compute_acceleration_reference(&view, &softened, NULL, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    NBodySimProVector3 acceleration;
    n_body_sim_pro_particle_system_acceleration(particle_system, 1, &acceleration, &error);

    /* With eps = 1e-2 the softened force magnitude is bounded well below
     * the unsoftened 1/r^2 = 1e16, and particle 1 is pulled toward the
     * origin (negative x). */
    N_BODY_SIM_PRO_ASSERT(fabs(acceleration.x) < 1.0e6);
    N_BODY_SIM_PRO_ASSERT(acceleration.x < 0.0);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_mutual_force_antisymmetry(void) {
    NBodySimProParticleSystem* particle_system = make_two_particle_system();
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);
    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.3);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProVector3 acceleration_0;
    NBodySimProVector3 acceleration_1;
    n_body_sim_pro_particle_system_acceleration(particle_system, 0, &acceleration_0, &error);
    n_body_sim_pro_particle_system_acceleration(particle_system, 1, &acceleration_1, &error);

    /* Equal masses: m_i a_i must be equal and opposite. */
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.x, -acceleration_1.x, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.y, -acceleration_1.y, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(acceleration_0.z, -acceleration_1.z, 1e-15);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_analytic_two_body_acceleration);
    N_BODY_SIM_PRO_TEST_RUN(test_self_force_is_excluded);
    N_BODY_SIM_PRO_TEST_RUN(test_softening_bounds_force);
    N_BODY_SIM_PRO_TEST_RUN(test_mutual_force_antisymmetry);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
