#include "hpcsim/core/particle_system.h"
#include "test_harness.h"

#include <stdint.h>

static void test_creation_and_capacity(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(128);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_capacity(particle_system), 128);
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_particle_count(particle_system), 0);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_zero_capacity_rejected(void) {
    HPCSIM_ASSERT(hpcsim_particle_system_create(0) == NULL);
}

static void test_component_roundtrip(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(4);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);

    HPCSIM_ASSERT(hpcsim_particle_system_set_particle_count(particle_system, 3, &error) ==
                  HPCSIM_STATUS_OK);

    const HpcsimVector3 position = {1.5, -2.5, 3.5};
    const HpcsimVector3 velocity = {0.1, 0.2, 0.3};
    const HpcsimVector3 acceleration = {9.8, 0.0, -9.8};
    HPCSIM_ASSERT(hpcsim_particle_system_set_position(particle_system, 1, position, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_set_velocity(particle_system, 1, velocity, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_set_acceleration(particle_system, 1, acceleration,
                                                          &error) == HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_set_mass(particle_system, 1, 2.0, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimVector3 read_position = {0, 0, 0};
    HpcsimVector3 read_velocity = {0, 0, 0};
    HpcsimVector3 read_acceleration = {0, 0, 0};
    double read_mass = 0.0;
    HPCSIM_ASSERT(hpcsim_particle_system_position(particle_system, 1, &read_position, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_velocity(particle_system, 1, &read_velocity, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_acceleration(particle_system, 1, &read_acceleration,
                                                      &error) == HPCSIM_STATUS_OK);
    HPCSIM_ASSERT(hpcsim_particle_system_mass(particle_system, 1, &read_mass, &error) ==
                  HPCSIM_STATUS_OK);

    HPCSIM_ASSERT_NEAR(read_position.x, position.x, 1e-15);
    HPCSIM_ASSERT_NEAR(read_position.y, position.y, 1e-15);
    HPCSIM_ASSERT_NEAR(read_position.z, position.z, 1e-15);
    HPCSIM_ASSERT_NEAR(read_velocity.x, velocity.x, 1e-15);
    HPCSIM_ASSERT_NEAR(read_velocity.y, velocity.y, 1e-15);
    HPCSIM_ASSERT_NEAR(read_velocity.z, velocity.z, 1e-15);
    HPCSIM_ASSERT_NEAR(read_acceleration.x, acceleration.x, 1e-15);
    HPCSIM_ASSERT_NEAR(read_acceleration.y, acceleration.y, 1e-15);
    HPCSIM_ASSERT_NEAR(read_acceleration.z, acceleration.z, 1e-15);
    HPCSIM_ASSERT_NEAR(read_mass, 2.0, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_bounds_checking(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HPCSIM_ASSERT(hpcsim_particle_system_set_particle_count(particle_system, 2, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimVector3 position = {1, 1, 1};
    HpcsimVector3 read = {0, 0, 0};
    HPCSIM_ASSERT(hpcsim_particle_system_set_position(particle_system, 2, position, &error) ==
                  HPCSIM_STATUS_INVALID_ARGUMENT);
    HPCSIM_ASSERT(hpcsim_particle_system_position(particle_system, 5, &read, &error) ==
                  HPCSIM_STATUS_INVALID_ARGUMENT);
    HPCSIM_ASSERT(hpcsim_particle_system_set_particle_count(particle_system, 99, &error) ==
                  HPCSIM_STATUS_INVALID_ARGUMENT);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_storage_alignment(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(16);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    const double* buffers[] = {
        hpcsim_particle_system_positions_x(particle_system),
        hpcsim_particle_system_positions_y(particle_system),
        hpcsim_particle_system_positions_z(particle_system),
        hpcsim_particle_system_velocities_x(particle_system),
        hpcsim_particle_system_velocities_y(particle_system),
        hpcsim_particle_system_velocities_z(particle_system),
        hpcsim_particle_system_accelerations_x(particle_system),
        hpcsim_particle_system_accelerations_y(particle_system),
        hpcsim_particle_system_accelerations_z(particle_system),
        hpcsim_particle_system_masses(particle_system),
    };
    for (size_t index = 0; index < sizeof(buffers) / sizeof(buffers[0]); ++index) {
        HPCSIM_ASSERT(buffers[index] != NULL);
        if (buffers[index] != NULL) {
            HPCSIM_ASSERT(((uintptr_t)buffers[index] % HPCSIM_PARTICLE_SYSTEM_ALIGNMENT) == 0);
        }
    }
    hpcsim_particle_system_destroy(particle_system);
}

static void test_reserve_preserves_particles(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(2);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HPCSIM_ASSERT(hpcsim_particle_system_set_particle_count(particle_system, 2, &error) ==
                  HPCSIM_STATUS_OK);
    const HpcsimVector3 position = {7.0, 8.0, 9.0};
    HPCSIM_ASSERT(hpcsim_particle_system_set_position(particle_system, 0, position, &error) ==
                  HPCSIM_STATUS_OK);

    HPCSIM_ASSERT(hpcsim_particle_system_reserve(particle_system, 1000, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_capacity(particle_system), 1000);
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_particle_count(particle_system), 2);

    HpcsimVector3 read = {0, 0, 0};
    HPCSIM_ASSERT(hpcsim_particle_system_position(particle_system, 0, &read, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_NEAR(read.x, position.x, 1e-15);
    HPCSIM_ASSERT_NEAR(read.y, position.y, 1e-15);
    HPCSIM_ASSERT_NEAR(read.z, position.z, 1e-15);

    hpcsim_particle_system_destroy(particle_system);
}

static void test_view_snapshot(void) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(8);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HPCSIM_ASSERT(hpcsim_particle_system_set_particle_count(particle_system, 5, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimParticleSystemView view;
    HPCSIM_ASSERT(hpcsim_particle_system_view(particle_system, &view, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_EQ_SIZE(view.particle_count, 5);
    HPCSIM_ASSERT(view.positions_x == hpcsim_particle_system_positions_x(particle_system));
    HPCSIM_ASSERT(view.masses == hpcsim_particle_system_masses(particle_system));

    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_creation_and_capacity);
    HPCSIM_TEST_RUN(test_zero_capacity_rejected);
    HPCSIM_TEST_RUN(test_component_roundtrip);
    HPCSIM_TEST_RUN(test_bounds_checking);
    HPCSIM_TEST_RUN(test_storage_alignment);
    HPCSIM_TEST_RUN(test_reserve_preserves_particles);
    HPCSIM_TEST_RUN(test_view_snapshot);
    return HPCSIM_TEST_SUITE_END();
}
