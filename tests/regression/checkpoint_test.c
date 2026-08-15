#include "n_body_sim_pro/checkpoint/checkpoint.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/physics/integrator.h"
#include "test_harness.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Checkpoint roundtrip test.
 *
 * A saved checkpoint must reproduce the particle state and metadata exactly
 * when loaded into a fresh particle system.
 */

static const char* const CHECKPOINT_TEST_PATH = "n_body_sim_pro_test_checkpoint.hpcs";

static NBodySimProParticleSystem* make_system(size_t particle_count) {
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(particle_count);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return NULL;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {particle_count, 1234};
    n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY, &parameters,
                           &error);
    return particle_system;
}

static void test_checkpoint_roundtrip(void) {
    const size_t particle_count = 256;
    NBodySimProParticleSystem* source = make_system(particle_count);
    N_BODY_SIM_PRO_ASSERT(source != NULL);
    if (source == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    NBodySimProParticleSystemView source_view;
    n_body_sim_pro_particle_system_view(source, &source_view, &error);

    NBodySimProCheckpointHeader write_header = {0};
    write_header.magic = N_BODY_SIM_PRO_CHECKPOINT_MAGIC;
    write_header.version = N_BODY_SIM_PRO_CHECKPOINT_VERSION;
    write_header.particle_count = particle_count;
    write_header.simulation_time = 12.5;
    write_header.timestep = 0.001;
    write_header.integrator = N_BODY_SIM_PRO_INTEGRATOR_LEAPFROG;
    write_header.theta = 0.7;
    write_header.barnes_hut_enabled = 1;
    write_header.random_seed = 999;
    write_header.preset = N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY;

    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_checkpoint_write(CHECKPOINT_TEST_PATH, &source_view,
                                          &write_header, &error) == N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProParticleSystem* target = n_body_sim_pro_particle_system_create(particle_count);
    N_BODY_SIM_PRO_ASSERT(target != NULL);
    if (target != NULL) {
        NBodySimProCheckpointHeader read_header = {0};
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_checkpoint_read(CHECKPOINT_TEST_PATH, &read_header, target,
                                             &error) == N_BODY_SIM_PRO_STATUS_OK);
        N_BODY_SIM_PRO_ASSERT_EQ_SIZE(read_header.particle_count, particle_count);
        N_BODY_SIM_PRO_ASSERT_NEAR(read_header.simulation_time, 12.5, 1e-15);
        N_BODY_SIM_PRO_ASSERT_NEAR(read_header.timestep, 0.001, 1e-15);
        N_BODY_SIM_PRO_ASSERT(read_header.integrator == N_BODY_SIM_PRO_INTEGRATOR_LEAPFROG);
        N_BODY_SIM_PRO_ASSERT(read_header.barnes_hut_enabled == 1);
        N_BODY_SIM_PRO_ASSERT(read_header.random_seed == 999);
        N_BODY_SIM_PRO_ASSERT(read_header.preset == N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY);

        NBodySimProParticleSystemView target_view;
        n_body_sim_pro_particle_system_view(target, &target_view, &error);
        int identical = 1;
        for (size_t i = 0; i < particle_count; ++i) {
            if (source_view.positions_x[i] != target_view.positions_x[i] ||
                source_view.velocities_y[i] != target_view.velocities_y[i] ||
                source_view.accelerations_z[i] != target_view.accelerations_z[i] ||
                source_view.masses[i] != target_view.masses[i]) {
                identical = 0;
                break;
            }
        }
        N_BODY_SIM_PRO_ASSERT(identical);

        n_body_sim_pro_particle_system_destroy(target);
    }

    n_body_sim_pro_particle_system_destroy(source);
    remove(CHECKPOINT_TEST_PATH);
}

static void test_checkpoint_peek(void) {
    const size_t particle_count = 64;
    NBodySimProParticleSystem* source = make_system(particle_count);
    N_BODY_SIM_PRO_ASSERT(source != NULL);
    if (source == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView source_view;
    n_body_sim_pro_particle_system_view(source, &source_view, &error);

    NBodySimProCheckpointHeader write_header = {0};
    write_header.magic = N_BODY_SIM_PRO_CHECKPOINT_MAGIC;
    write_header.version = N_BODY_SIM_PRO_CHECKPOINT_VERSION;
    write_header.particle_count = particle_count;
    write_header.simulation_time = 3.0;
    write_header.timestep = 0.002;
    write_header.integrator = N_BODY_SIM_PRO_INTEGRATOR_EULER;
    write_header.theta = 0.5;
    write_header.barnes_hut_enabled = 0;
    write_header.random_seed = 7;
    write_header.preset = N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD;
    n_body_sim_pro_checkpoint_write(CHECKPOINT_TEST_PATH, &source_view, &write_header, &error);

    NBodySimProCheckpointHeader peeked = {0};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_checkpoint_peek(CHECKPOINT_TEST_PATH, &peeked, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(peeked.particle_count, particle_count);
    N_BODY_SIM_PRO_ASSERT_NEAR(peeked.simulation_time, 3.0, 1e-15);

    n_body_sim_pro_particle_system_destroy(source);
    remove(CHECKPOINT_TEST_PATH);
}

static void test_checkpoint_rejects_garbage(void) {
    const char* const garbage_path = "n_body_sim_pro_test_garbage.hpcs";
    FILE* file = fopen(garbage_path, "wb");
    N_BODY_SIM_PRO_ASSERT(file != NULL);
    if (file != NULL) {
        const char junk[] = "this is not a checkpoint";
        fwrite(junk, sizeof(junk), 1, file);
        fclose(file);
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProCheckpointHeader header = {0};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_checkpoint_peek(garbage_path, &header, &error) ==
                  N_BODY_SIM_PRO_STATUS_INVALID_STATE);
    remove(garbage_path);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_checkpoint_roundtrip);
    N_BODY_SIM_PRO_TEST_RUN(test_checkpoint_peek);
    N_BODY_SIM_PRO_TEST_RUN(test_checkpoint_rejects_garbage);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
