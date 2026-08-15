#include "hpcsim/checkpoint/checkpoint.h"
#include "hpcsim/generation/presets.h"
#include "hpcsim/physics/integrator.h"
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

static const char* const CHECKPOINT_TEST_PATH = "hpcsim_test_checkpoint.hpcs";

static HpcsimParticleSystem* make_system(size_t particle_count) {
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(particle_count);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return NULL;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {particle_count, 1234};
    hpcsim_preset_generate(particle_system, HPCSIM_PRESET_SPIRAL_GALAXY, &parameters,
                           &error);
    return particle_system;
}

static void test_checkpoint_roundtrip(void) {
    const size_t particle_count = 256;
    HpcsimParticleSystem* source = make_system(particle_count);
    HPCSIM_ASSERT(source != NULL);
    if (source == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);

    HpcsimParticleSystemView source_view;
    hpcsim_particle_system_view(source, &source_view, &error);

    HpcsimCheckpointHeader write_header = {0};
    write_header.magic = HPCSIM_CHECKPOINT_MAGIC;
    write_header.version = HPCSIM_CHECKPOINT_VERSION;
    write_header.particle_count = particle_count;
    write_header.simulation_time = 12.5;
    write_header.timestep = 0.001;
    write_header.integrator = HPCSIM_INTEGRATOR_LEAPFROG;
    write_header.theta = 0.7;
    write_header.barnes_hut_enabled = 1;
    write_header.random_seed = 999;
    write_header.preset = HPCSIM_PRESET_SPIRAL_GALAXY;

    HPCSIM_ASSERT(hpcsim_checkpoint_write(CHECKPOINT_TEST_PATH, &source_view,
                                          &write_header, &error) == HPCSIM_STATUS_OK);

    HpcsimParticleSystem* target = hpcsim_particle_system_create(particle_count);
    HPCSIM_ASSERT(target != NULL);
    if (target != NULL) {
        HpcsimCheckpointHeader read_header = {0};
        HPCSIM_ASSERT(hpcsim_checkpoint_read(CHECKPOINT_TEST_PATH, &read_header, target,
                                             &error) == HPCSIM_STATUS_OK);
        HPCSIM_ASSERT_EQ_SIZE(read_header.particle_count, particle_count);
        HPCSIM_ASSERT_NEAR(read_header.simulation_time, 12.5, 1e-15);
        HPCSIM_ASSERT_NEAR(read_header.timestep, 0.001, 1e-15);
        HPCSIM_ASSERT(read_header.integrator == HPCSIM_INTEGRATOR_LEAPFROG);
        HPCSIM_ASSERT(read_header.barnes_hut_enabled == 1);
        HPCSIM_ASSERT(read_header.random_seed == 999);
        HPCSIM_ASSERT(read_header.preset == HPCSIM_PRESET_SPIRAL_GALAXY);

        HpcsimParticleSystemView target_view;
        hpcsim_particle_system_view(target, &target_view, &error);
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
        HPCSIM_ASSERT(identical);

        hpcsim_particle_system_destroy(target);
    }

    hpcsim_particle_system_destroy(source);
    remove(CHECKPOINT_TEST_PATH);
}

static void test_checkpoint_peek(void) {
    const size_t particle_count = 64;
    HpcsimParticleSystem* source = make_system(particle_count);
    HPCSIM_ASSERT(source != NULL);
    if (source == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView source_view;
    hpcsim_particle_system_view(source, &source_view, &error);

    HpcsimCheckpointHeader write_header = {0};
    write_header.magic = HPCSIM_CHECKPOINT_MAGIC;
    write_header.version = HPCSIM_CHECKPOINT_VERSION;
    write_header.particle_count = particle_count;
    write_header.simulation_time = 3.0;
    write_header.timestep = 0.002;
    write_header.integrator = HPCSIM_INTEGRATOR_EULER;
    write_header.theta = 0.5;
    write_header.barnes_hut_enabled = 0;
    write_header.random_seed = 7;
    write_header.preset = HPCSIM_PRESET_RANDOM_CLOUD;
    hpcsim_checkpoint_write(CHECKPOINT_TEST_PATH, &source_view, &write_header, &error);

    HpcsimCheckpointHeader peeked = {0};
    HPCSIM_ASSERT(hpcsim_checkpoint_peek(CHECKPOINT_TEST_PATH, &peeked, &error) ==
                  HPCSIM_STATUS_OK);
    HPCSIM_ASSERT_EQ_SIZE(peeked.particle_count, particle_count);
    HPCSIM_ASSERT_NEAR(peeked.simulation_time, 3.0, 1e-15);

    hpcsim_particle_system_destroy(source);
    remove(CHECKPOINT_TEST_PATH);
}

static void test_checkpoint_rejects_garbage(void) {
    const char* const garbage_path = "hpcsim_test_garbage.hpcs";
    FILE* file = fopen(garbage_path, "wb");
    HPCSIM_ASSERT(file != NULL);
    if (file != NULL) {
        const char junk[] = "this is not a checkpoint";
        fwrite(junk, sizeof(junk), 1, file);
        fclose(file);
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimCheckpointHeader header = {0};
    HPCSIM_ASSERT(hpcsim_checkpoint_peek(garbage_path, &header, &error) ==
                  HPCSIM_STATUS_INVALID_STATE);
    remove(garbage_path);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_checkpoint_roundtrip);
    HPCSIM_TEST_RUN(test_checkpoint_peek);
    HPCSIM_TEST_RUN(test_checkpoint_rejects_garbage);
    return HPCSIM_TEST_SUITE_END();
}
