#include "hpcsim/generation/presets.h"
#include "hpcsim/physics/gravity.h"
#include "hpcsim/simd/backend.h"
#include "hpcsim/simd/cpu.h"
#include "test_harness.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
 * SIMD correctness tests.
 *
 * Every SIMD backend must agree with the reference kernel within a defined
 * numerical tolerance for the same inputs. The AVX2 kernel reorders the
 * force sum, so bit equality is not expected; a relative tolerance of 1e-10
 * is far tighter than any physical application needs.
 */

enum { TEST_PARTICLE_COUNT = 300 };

static HpcsimParticleSystem* make_random_system(size_t particle_count, uint64_t seed) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(particle_count);
    if (particle_system == NULL) {
        return NULL;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {particle_count, seed};
    hpcsim_preset_generate(particle_system, HPCSIM_PRESET_RANDOM_CLOUD, &parameters, &error);
    return particle_system;
}

static double maximum_relative_error(HpcsimParticleSystemView* view,
                                     const double reference[3 * TEST_PARTICLE_COUNT],
                                     size_t particle_count) {
    double maximum_error = 0.0;
    for (size_t i = 0; i < particle_count; ++i) {
        const double candidate[3] = {view->accelerations_x[i], view->accelerations_y[i],
                                     view->accelerations_z[i]};
        for (size_t component = 0; component < 3; ++component) {
            const double magnitude = fabs(reference[3 * i + component]);
            const double scale = magnitude > 1.0e-12 ? magnitude : 1.0;
            const double error =
                fabs(candidate[component] - reference[3 * i + component]) / scale;
            if (error > maximum_error) {
                maximum_error = error;
            }
        }
    }
    return maximum_error;
}

static void test_avx2_matches_reference_with_softening(void) {
    HpcsimCpuFeatures features = hpcsim_cpu_detect_features();
    if (!features.has_avx2) {
        HPCSIM_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    HpcsimParticleSystem* particle_system = make_random_system(particle_count, 7);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.02);

    double reference[3 * TEST_PARTICLE_COUNT];
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    hpcsim_gravity_compute_acceleration_avx2(&view, &gravity, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    HPCSIM_ASSERT(relative_error < 1.0e-10);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_avx2_matches_reference_without_softening(void) {
    HpcsimCpuFeatures features = hpcsim_cpu_detect_features();
    if (!features.has_avx2) {
        HPCSIM_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    HpcsimParticleSystem* particle_system = make_random_system(particle_count, 7);
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

    double reference[3 * TEST_PARTICLE_COUNT];
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    hpcsim_gravity_compute_acceleration_avx2(&view, &gravity, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    HPCSIM_ASSERT(relative_error < 1.0e-10);

    /* Without softening the self-lane guard must still produce finite values. */
    HPCSIM_ASSERT(!isnan(view.accelerations_x[0]));
    hpcsim_particle_system_destroy(particle_system);
}

static void test_openmp_avx2_matches_reference(void) {
    HpcsimCpuFeatures features = hpcsim_cpu_detect_features();
    if (!features.has_avx2) {
        HPCSIM_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    HpcsimParticleSystem* particle_system = make_random_system(particle_count, 7);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.02);

    double reference[3 * TEST_PARTICLE_COUNT];
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    hpcsim_gravity_compute_acceleration_openmp_avx2(&view, &gravity, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    HPCSIM_ASSERT(relative_error < 1.0e-10);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_backend_selection(void) {
    HpcsimCpuFeatures features = hpcsim_cpu_detect_features();
    const HpcsimSimdBackend backend = hpcsim_simd_best_available_backend(&features);
    HPCSIM_ASSERT(backend == HPCSIM_SIMD_BACKEND_SCALAR ||
                  backend == HPCSIM_SIMD_BACKEND_AVX2);
    HPCSIM_ASSERT(hpcsim_simd_backend_string(backend) != NULL);
    if (features.has_avx2 && features.has_fma) {
        HPCSIM_ASSERT(backend == HPCSIM_SIMD_BACKEND_AVX2);
    }
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_avx2_matches_reference_with_softening);
    HPCSIM_TEST_RUN(test_avx2_matches_reference_without_softening);
    HPCSIM_TEST_RUN(test_openmp_avx2_matches_reference);
    HPCSIM_TEST_RUN(test_backend_selection);
    return HPCSIM_TEST_SUITE_END();
}
