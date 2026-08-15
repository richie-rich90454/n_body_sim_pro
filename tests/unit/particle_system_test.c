#include "n_body_sim_pro/core/particle_system.h"
#include "test_harness.h"

#include <stdint.h>

static void test_creation_and_capacity(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(128);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_capacity(particle_system), 128);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_particle_count(particle_system), 0);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_zero_capacity_rejected(void) {
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_create(0) == NULL);
}

static void test_component_roundtrip(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(4);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_particle_count(particle_system, 3, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    const NBodySimProVector3 position = {1.5, -2.5, 3.5};
    const NBodySimProVector3 velocity = {0.1, 0.2, 0.3};
    const NBodySimProVector3 acceleration = {9.8, 0.0, -9.8};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_position(particle_system, 1, position, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_velocity(particle_system, 1, velocity, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_acceleration(particle_system, 1, acceleration,
                                                          &error) == N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_mass(particle_system, 1, 2.0, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProVector3 read_position = {0, 0, 0};
    NBodySimProVector3 read_velocity = {0, 0, 0};
    NBodySimProVector3 read_acceleration = {0, 0, 0};
    double read_mass = 0.0;
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_position(particle_system, 1, &read_position, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_velocity(particle_system, 1, &read_velocity, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_acceleration(particle_system, 1, &read_acceleration,
                                                      &error) == N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_mass(particle_system, 1, &read_mass, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    N_BODY_SIM_PRO_ASSERT_NEAR(read_position.x, position.x, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_position.y, position.y, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_position.z, position.z, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_velocity.x, velocity.x, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_velocity.y, velocity.y, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_velocity.z, velocity.z, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_acceleration.x, acceleration.x, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_acceleration.y, acceleration.y, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_acceleration.z, acceleration.z, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read_mass, 2.0, 1e-15);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_bounds_checking(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(2);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_particle_count(particle_system, 2, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProVector3 position = {1, 1, 1};
    NBodySimProVector3 read = {0, 0, 0};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_position(particle_system, 2, position, &error) ==
                  N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_position(particle_system, 5, &read, &error) ==
                  N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_particle_count(particle_system, 99, &error) ==
                  N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_storage_alignment(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(16);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    const double* buffers[] = {
        n_body_sim_pro_particle_system_positions_x(particle_system),
        n_body_sim_pro_particle_system_positions_y(particle_system),
        n_body_sim_pro_particle_system_positions_z(particle_system),
        n_body_sim_pro_particle_system_velocities_x(particle_system),
        n_body_sim_pro_particle_system_velocities_y(particle_system),
        n_body_sim_pro_particle_system_velocities_z(particle_system),
        n_body_sim_pro_particle_system_accelerations_x(particle_system),
        n_body_sim_pro_particle_system_accelerations_y(particle_system),
        n_body_sim_pro_particle_system_accelerations_z(particle_system),
        n_body_sim_pro_particle_system_masses(particle_system),
    };
    for (size_t index = 0; index < sizeof(buffers) / sizeof(buffers[0]); ++index) {
        N_BODY_SIM_PRO_ASSERT(buffers[index] != NULL);
        if (buffers[index] != NULL) {
            N_BODY_SIM_PRO_ASSERT(((uintptr_t)buffers[index] % N_BODY_SIM_PRO_PARTICLE_SYSTEM_ALIGNMENT) == 0);
        }
    }
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_reserve_preserves_particles(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(2);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_particle_count(particle_system, 2, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    const NBodySimProVector3 position = {7.0, 8.0, 9.0};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_position(particle_system, 0, position, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_reserve(particle_system, 1000, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_capacity(particle_system), 1000);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_particle_count(particle_system), 2);

    NBodySimProVector3 read = {0, 0, 0};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_position(particle_system, 0, &read, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_NEAR(read.x, position.x, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read.y, position.y, 1e-15);
    N_BODY_SIM_PRO_ASSERT_NEAR(read.z, position.z, 1e-15);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_view_snapshot(void) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(8);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_set_particle_count(particle_system, 5, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProParticleSystemView view;
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_particle_system_view(particle_system, &view, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(view.particle_count, 5);
    N_BODY_SIM_PRO_ASSERT(view.positions_x == n_body_sim_pro_particle_system_positions_x(particle_system));
    N_BODY_SIM_PRO_ASSERT(view.masses == n_body_sim_pro_particle_system_masses(particle_system));

    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_creation_and_capacity);
    N_BODY_SIM_PRO_TEST_RUN(test_zero_capacity_rejected);
    N_BODY_SIM_PRO_TEST_RUN(test_component_roundtrip);
    N_BODY_SIM_PRO_TEST_RUN(test_bounds_checking);
    N_BODY_SIM_PRO_TEST_RUN(test_storage_alignment);
    N_BODY_SIM_PRO_TEST_RUN(test_reserve_preserves_particles);
    N_BODY_SIM_PRO_TEST_RUN(test_view_snapshot);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
