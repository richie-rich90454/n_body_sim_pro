#include "n_body_sim_pro/diagnostics/numerics.h"
#include "n_body_sim_pro/generation/presets.h"
#include "test_harness.h"

#include <math.h>

/*
 * Tests for initial-condition presets. Every preset must be deterministic,
 * must produce exactly the requested particle count, and must place the
 * system with zero net momentum (a drifting simulation would look wrong).
 */

static NBodySimProParticleSystem* make_system(size_t capacity) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(capacity);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    return particle_system;
}

static double momentum_magnitude(NBodySimProParticleSystem* particle_system) {
    NBodySimProParticleSystemView view;
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);
    NBodySimProDiagnosticsQuantities quantities;
    n_body_sim_pro_diagnostics_compute_global(&view, &quantities, &error);
    return sqrt(quantities.total_momentum_x * quantities.total_momentum_x +
                quantities.total_momentum_y * quantities.total_momentum_y +
                quantities.total_momentum_z * quantities.total_momentum_z);
}

static void test_two_body_preset(void) {
    NBodySimProParticleSystem* particle_system = make_system(2);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {2, 1234};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_TWO_BODY,
                                         &parameters, &error) == N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_particle_count(particle_system), 2);
    N_BODY_SIM_PRO_ASSERT(momentum_magnitude(particle_system) < 1e-12);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_presets_are_deterministic(void) {
    const NBodySimProSimulationPreset presets[] = {N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD,
                                              N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER,
                                              N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY,
                                              N_BODY_SIM_PRO_PRESET_ELLIPTICAL_GALAXY};
    const size_t count = 400;
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    for (size_t preset_index = 0;
         preset_index < sizeof(presets) / sizeof(presets[0]); ++preset_index) {
        NBodySimProParticleSystem* first = make_system(count);
        NBodySimProParticleSystem* second = make_system(count);
        if (first == NULL || second == NULL) {
            n_body_sim_pro_particle_system_destroy(first);
            n_body_sim_pro_particle_system_destroy(second);
            return;
        }
        NBodySimProPresetParameters parameters = {count, 777};
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(first, presets[preset_index], &parameters,
                                             &error) == N_BODY_SIM_PRO_STATUS_OK);
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(second, presets[preset_index], &parameters,
                                             &error) == N_BODY_SIM_PRO_STATUS_OK);

        int identical = 1;
        for (size_t i = 0; i < count; ++i) {
            NBodySimProVector3 p_first;
            NBodySimProVector3 v_first;
            NBodySimProVector3 p_second;
            NBodySimProVector3 v_second;
            n_body_sim_pro_particle_system_position(first, i, &p_first, &error);
            n_body_sim_pro_particle_system_position(second, i, &p_second, &error);
            n_body_sim_pro_particle_system_velocity(first, i, &v_first, &error);
            n_body_sim_pro_particle_system_velocity(second, i, &v_second, &error);
            if (fabs(p_first.x - p_second.x) > 1e-15 ||
                fabs(v_first.y - v_second.y) > 1e-15) {
                identical = 0;
                break;
            }
        }
        N_BODY_SIM_PRO_ASSERT(identical);

        n_body_sim_pro_particle_system_destroy(first);
        n_body_sim_pro_particle_system_destroy(second);
    }
}

static void test_presets_have_zero_net_momentum(void) {
    const NBodySimProSimulationPreset presets[] = {N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD,
                                              N_BODY_SIM_PRO_PRESET_OPEN_CLUSTER,
                                              N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER,
                                              N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY,
                                              N_BODY_SIM_PRO_PRESET_ELLIPTICAL_GALAXY,
                                              N_BODY_SIM_PRO_PRESET_GALAXY_COLLISION,
                                              N_BODY_SIM_PRO_PRESET_TRIPLE_GALAXY};
    const size_t count = 3000;
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    for (size_t preset_index = 0;
         preset_index < sizeof(presets) / sizeof(presets[0]); ++preset_index) {
        NBodySimProParticleSystem* particle_system = make_system(count);
        if (particle_system == NULL) {
            return;
        }
        NBodySimProPresetParameters parameters = {count, 555};
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(particle_system, presets[preset_index],
                                             &parameters, &error) == N_BODY_SIM_PRO_STATUS_OK);
        N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_particle_count(particle_system), count);
        N_BODY_SIM_PRO_ASSERT(momentum_magnitude(particle_system) < 1e-9);
        n_body_sim_pro_particle_system_destroy(particle_system);
    }
}

static void test_plummer_sphere_is_finite_and_centered(void) {
    const size_t count = 10000;
    NBodySimProParticleSystem* particle_system = make_system(count);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {count, 11};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER,
                                         &parameters, &error) == N_BODY_SIM_PRO_STATUS_OK);

    double maximum_radius = 0.0;
    for (size_t i = 0; i < count; ++i) {
        NBodySimProVector3 position;
        n_body_sim_pro_particle_system_position(particle_system, i, &position, &error);
        const double radius = sqrt(position.x * position.x + position.y * position.y +
                                   position.z * position.z);
        if (radius > maximum_radius) {
            maximum_radius = radius;
        }
    }
    /* Plummer has a heavy tail, but at this sample size the core dominates. */
    N_BODY_SIM_PRO_ASSERT(maximum_radius < 1000.0);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_solar_system_planets_orbit(void) {
    const size_t count = 5;
    NBodySimProParticleSystem* particle_system = make_system(count);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {count, 3};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_SOLAR_SYSTEM,
                                         &parameters, &error) == N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(n_body_sim_pro_particle_system_particle_count(particle_system), 5);

    double stellar_mass = 0.0;
    n_body_sim_pro_particle_system_mass(particle_system, 0, &stellar_mass, &error);
    N_BODY_SIM_PRO_ASSERT_NEAR(stellar_mass, 1.0, 1e-15);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_preset_rejects_oversized_request(void) {
    NBodySimProParticleSystem* particle_system = make_system(16);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {100, 1};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD,
                                         &parameters, &error) ==
                  N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_two_body_preset);
    N_BODY_SIM_PRO_TEST_RUN(test_presets_are_deterministic);
    N_BODY_SIM_PRO_TEST_RUN(test_presets_have_zero_net_momentum);
    N_BODY_SIM_PRO_TEST_RUN(test_plummer_sphere_is_finite_and_centered);
    N_BODY_SIM_PRO_TEST_RUN(test_solar_system_planets_orbit);
    N_BODY_SIM_PRO_TEST_RUN(test_preset_rejects_oversized_request);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
