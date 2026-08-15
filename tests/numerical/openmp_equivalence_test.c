#include "hpcsim/generation/presets.h"
#include "hpcsim/physics/gravity.h"
#include "hpcsim/threading/threading.h"
#include "test_harness.h"

#include <math.h>
#include <stdint.h>

/*
 * Validates the OpenMP kernel against the reference kernel.
 *
 * Because each particle's force sum is accumulated in the same serial order
 * in both kernels, the OpenMP results must be bit-identical to the
 * reference for the same inputs. This test asserts that equality (within a
 * tight epsilon to be robust across compiler reorderings).
 */

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

static void test_openmp_matches_reference(void) {
    const size_t particle_count = 512;
    HpcsimParticleSystem* particle_system = make_random_system(particle_count, 42);
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

    double reference[3 * particle_count];
    double parallel[3 * particle_count];

    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    hpcsim_gravity_compute_acceleration_openmp(&view, &gravity, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        parallel[3 * i + 0] = view.accelerations_x[i];
        parallel[3 * i + 1] = view.accelerations_y[i];
        parallel[3 * i + 2] = view.accelerations_z[i];
    }

    for (size_t i = 0; i < 3 * particle_count; ++i) {
        HPCSIM_ASSERT(fabs(reference[i] - parallel[i]) <= 1e-15);
    }

    hpcsim_particle_system_destroy(particle_system);
}

static void test_thread_count_configuration(void) {
    if (!hpcsim_threading_openmp_available()) {
        HPCSIM_ASSERT(hpcsim_threading_active_thread_count() == 1);
        HPCSIM_ASSERT(hpcsim_threading_available_thread_count() == 1);
        return;
    }
    const int available = hpcsim_threading_available_thread_count();
    HPCSIM_ASSERT(available >= 1);
    hpcsim_threading_set_thread_count(2);
    HPCSIM_ASSERT(hpcsim_threading_thread_count() >= 2);
    hpcsim_threading_set_thread_count(-1);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_openmp_matches_reference);
    HPCSIM_TEST_RUN(test_thread_count_configuration);
    return HPCSIM_TEST_SUITE_END();
}
