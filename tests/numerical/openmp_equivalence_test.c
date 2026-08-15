#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/physics/gravity.h"
#include "n_body_sim_pro/threading/threading.h"
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

static NBodySimProParticleSystem* make_random_system(size_t particle_count, uint64_t seed) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(particle_count);
    if (particle_system == NULL) {
        return NULL;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {particle_count, seed};
    n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD, &parameters, &error);
    return particle_system;
}

static void test_openmp_matches_reference(void) {
    enum { PARTICLE_COUNT = 512 };
    const size_t particle_count = PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 42);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.02);

    double reference[3 * PARTICLE_COUNT];
    double parallel[3 * PARTICLE_COUNT];

    n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_openmp(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        parallel[3 * i + 0] = view.accelerations_x[i];
        parallel[3 * i + 1] = view.accelerations_y[i];
        parallel[3 * i + 2] = view.accelerations_z[i];
    }

    for (size_t i = 0; i < 3 * particle_count; ++i) {
        N_BODY_SIM_PRO_ASSERT(fabs(reference[i] - parallel[i]) <= 1e-15);
    }

    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_thread_count_configuration(void) {
    if (!n_body_sim_pro_threading_openmp_available()) {
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_threading_active_thread_count() == 1);
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_threading_available_thread_count() == 1);
        return;
    }
    const int available = n_body_sim_pro_threading_available_thread_count();
    N_BODY_SIM_PRO_ASSERT(available >= 1);
    n_body_sim_pro_threading_set_thread_count(2);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_threading_thread_count() >= 2);
    n_body_sim_pro_threading_set_thread_count(-1);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_openmp_matches_reference);
    N_BODY_SIM_PRO_TEST_RUN(test_thread_count_configuration);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
