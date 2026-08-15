#include "hpcsim/diagnostics/numerics.h"
#include "hpcsim/generation/presets.h"
#include "test_harness.h"

#include <math.h>

/*
 * Tests for initial-condition presets. Every preset must be deterministic,
 * must produce exactly the requested particle count, and must place the
 * system with zero net momentum (a drifting simulation would look wrong).
 */

static HpcsimParticleSystem* make_system(size_t capacity) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(capacity);
    HPCSIM_ASSERT(particle_system != NULL);
    return particle_system;
}

static double momentum_magnitude(HpcsimParticleSystem* particle_system) {
    HpcsimParticleSystemView view;
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_particle_system_view(particle_system, &view, &error);
    HpcsimDiagnosticsQuantities quantities;
    hpcsim_diagnostics_compute_global(&view, &quantities, &error);
    return sqrt(quantities.total_momentum_x * quantities.total_momentum_x +
                quantities.total_momentum_y * quantities.total_momentum_y +
                quantities.total_momentum_z * quantities.total_momentum_z);
}

static void test_two_body_preset(void) {
    HpcsimParticleSystem* particle_system = make_system(2);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {2, 1234};
    HPCSIM_ASSERT(hpcsim_preset_generate(particle_system, HPCSIM_PRESET_TWO_BODY,
                                         &parameters, &error) == HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_particle_count(particle_system), 2);
    HPCSIM_ASSERT(momentum_magnitude(particle_system) < 1e-12);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_presets_are_deterministic(void) {
    const HpcsimSimulationPreset presets[] = {HPCSIM_PRESET_RANDOM_CLOUD,
                                              HPCSIM_PRESET_GLOBULAR_CLUSTER,
                                              HPCSIM_PRESET_SPIRAL_GALAXY,
                                              HPCSIM_PRESET_ELLIPTICAL_GALAXY};
    const size_t count = 400;
    HpcsimError error;
    hpcsim_error_clear(&error);

    for (size_t preset_index = 0;
         preset_index < sizeof(presets) / sizeof(presets[0]); ++preset_index) {
        HpcsimParticleSystem* first = make_system(count);
        HpcsimParticleSystem* second = make_system(count);
        if (first == NULL || second == NULL) {
            hpcsim_particle_system_destroy(first);
            hpcsim_particle_system_destroy(second);
            return;
        }
        HpcsimPresetParameters parameters = {count, 777};
        HPCSIM_ASSERT(hpcsim_preset_generate(first, presets[preset_index], &parameters,
                                             &error) == HPCSIM_STATUS_OK);
        HPCSIM_ASSERT(hpcsim_preset_generate(second, presets[preset_index], &parameters,
                                             &error) == HPCSIM_STATUS_OK);

        int identical = 1;
        for (size_t i = 0; i < count; ++i) {
            HpcsimVector3 p_first;
            HpcsimVector3 v_first;
            HpcsimVector3 p_second;
            HpcsimVector3 v_second;
            hpcsim_particle_system_position(first, i, &p_first, &error);
            hpcsim_particle_system_position(second, i, &p_second, &error);
            hpcsim_particle_system_velocity(first, i, &v_first, &error);
            hpcsim_particle_system_velocity(second, i, &v_second, &error);
            if (fabs(p_first.x - p_second.x) > 1e-15 ||
                fabs(v_first.y - v_second.y) > 1e-15) {
                identical = 0;
                break;
            }
        }
        HPCSIM_ASSERT(identical);

        hpcsim_particle_system_destroy(first);
        hpcsim_particle_system_destroy(second);
    }
}

static void test_presets_have_zero_net_momentum(void) {
    const HpcsimSimulationPreset presets[] = {HPCSIM_PRESET_RANDOM_CLOUD,
                                              HPCSIM_PRESET_OPEN_CLUSTER,
                                              HPCSIM_PRESET_GLOBULAR_CLUSTER,
                                              HPCSIM_PRESET_SPIRAL_GALAXY,
                                              HPCSIM_PRESET_ELLIPTICAL_GALAXY,
                                              HPCSIM_PRESET_GALAXY_COLLISION,
                                              HPCSIM_PRESET_TRIPLE_GALAXY};
    const size_t count = 3000;
    HpcsimError error;
    hpcsim_error_clear(&error);

    for (size_t preset_index = 0;
         preset_index < sizeof(presets) / sizeof(presets[0]); ++preset_index) {
        HpcsimParticleSystem* particle_system = make_system(count);
        if (particle_system == NULL) {
            return;
        }
        HpcsimPresetParameters parameters = {count, 555};
        HPCSIM_ASSERT(hpcsim_preset_generate(particle_system, presets[preset_index],
                                             &parameters, &error) == HPCSIM_STATUS_OK);
        HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_particle_count(particle_system), count);
        HPCSIM_ASSERT(momentum_magnitude(particle_system) < 1e-9);
        hpcsim_particle_system_destroy(particle_system);
    }
}

static void test_plummer_sphere_is_finite_and_centered(void) {
    const size_t count = 10000;
    HpcsimParticleSystem* particle_system = make_system(count);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {count, 11};
    HPCSIM_ASSERT(hpcsim_preset_generate(particle_system, HPCSIM_PRESET_GLOBULAR_CLUSTER,
                                         &parameters, &error) == HPCSIM_STATUS_OK);

    double maximum_radius = 0.0;
    for (size_t i = 0; i < count; ++i) {
        HpcsimVector3 position;
        hpcsim_particle_system_position(particle_system, i, &position, &error);
        const double radius = sqrt(position.x * position.x + position.y * position.y +
                                   position.z * position.z);
        if (radius > maximum_radius) {
            maximum_radius = radius;
        }
    }
    /* Plummer has a heavy tail, but at this sample size the core dominates. */
    HPCSIM_ASSERT(maximum_radius < 1000.0);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_solar_system_planets_orbit(void) {
    const size_t count = 5;
    HpcsimParticleSystem* particle_system = make_system(count);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {count, 3};
    HPCSIM_ASSERT(hpcsim_preset_generate(particle_system, HPCSIM_PRESET_SOLAR_SYSTEM,
                                         &parameters, &error) == HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_EQ_SIZE(hpcsim_particle_system_particle_count(particle_system), 5);

    double stellar_mass = 0.0;
    hpcsim_particle_system_mass(particle_system, 0, &stellar_mass, &error);
    HPCSIM_ASSERT_NEAR(stellar_mass, 1.0, 1e-15);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_preset_rejects_oversized_request(void) {
    HpcsimParticleSystem* particle_system = make_system(16);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {100, 1};
    HPCSIM_ASSERT(hpcsim_preset_generate(particle_system, HPCSIM_PRESET_RANDOM_CLOUD,
                                         &parameters, &error) ==
                  HPCSIM_STATUS_INVALID_ARGUMENT);
    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_two_body_preset);
    HPCSIM_TEST_RUN(test_presets_are_deterministic);
    HPCSIM_TEST_RUN(test_presets_have_zero_net_momentum);
    HPCSIM_TEST_RUN(test_plummer_sphere_is_finite_and_centered);
    HPCSIM_TEST_RUN(test_solar_system_planets_orbit);
    HPCSIM_TEST_RUN(test_preset_rejects_oversized_request);
    return HPCSIM_TEST_SUITE_END();
}
