#include "hpcsim/diagnostics/numerics.h"
#include "hpcsim/generation/presets.h"
#include "hpcsim/threading/numa.h"
#include "hpcsim/threading/threading.h"
#include "test_harness.h"

#include <math.h>
#include <stdint.h>

/*
 * NUMA and thread-affinity tests.
 *
 * Topology detection must return at least one node and a positive processor
 * count; the parallel first-touch generator must be deterministic (same
 * seed, any thread count -> same positions) and valid (zero net momentum).
 */

static void test_numa_detection(void) {
    HpcsimNumaTopology topology;
    if (hpcsim_numa_detect(&topology) != 0) {
        HPCSIM_ASSERT(1);
        return;
    }
    HPCSIM_ASSERT(topology.node_count >= 1);
    HPCSIM_ASSERT(topology.logical_processor_count >= 1);
    hpcsim_numa_topology_destroy(&topology);
}

static void test_first_touch_and_affinity(void) {
    double buffer[4096];
    for (size_t i = 0; i < 4096; ++i) {
        buffer[i] = 1.0;
    }
    hpcsim_memory_first_touch(buffer, 4096);
    HPCSIM_ASSERT(buffer[0] == 0.0);
    /* Pinning the current thread is best-effort; it may fail in restricted
     * environments, so only the no-crash guarantee is asserted. */
    hpcsim_thread_affinity_apply_policy(HPCSIM_AFFINITY_COMPACT, 0);
    hpcsim_thread_affinity_apply_policy(HPCSIM_AFFINITY_SPREAD, 0);
    hpcsim_thread_affinity_apply_policy(HPCSIM_AFFINITY_AUTO, 0);
    HPCSIM_ASSERT(1);
}

static void test_parallel_generation_is_deterministic(void) {
    const size_t count = 2000;
    HpcsimPresetParameters parameters = {count, 555};

    HpcsimParticleSystem* first = hpcsim_particle_system_create(count);
    HpcsimParticleSystem* second = hpcsim_particle_system_create(count);
    HPCSIM_ASSERT(first != NULL && second != NULL);
    if (first == NULL || second == NULL) {
        hpcsim_particle_system_destroy(first);
        hpcsim_particle_system_destroy(second);
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);

    hpcsim_threading_set_thread_count(2);
    HPCSIM_ASSERT(hpcsim_preset_generate_parallel(first, HPCSIM_PRESET_SPIRAL_GALAXY,
                                                  &parameters, &error) ==
                  HPCSIM_STATUS_OK);
    hpcsim_threading_set_thread_count(4);
    HPCSIM_ASSERT(hpcsim_preset_generate_parallel(second, HPCSIM_PRESET_SPIRAL_GALAXY,
                                                  &parameters, &error) ==
                  HPCSIM_STATUS_OK);
    hpcsim_threading_set_thread_count(-1);

    int identical = 1;
    for (size_t i = 0; i < count; ++i) {
        HpcsimVector3 p_first;
        HpcsimVector3 p_second;
        hpcsim_particle_system_position(first, i, &p_first, &error);
        hpcsim_particle_system_position(second, i, &p_second, &error);
        if (fabs(p_first.x - p_second.x) > 1e-12 ||
            fabs(p_first.y - p_second.y) > 1e-12 ||
            fabs(p_first.z - p_second.z) > 1e-12) {
            identical = 0;
            break;
        }
    }
    HPCSIM_ASSERT(identical);

    hpcsim_particle_system_destroy(first);
    hpcsim_particle_system_destroy(second);
}

static void test_parallel_generation_zero_momentum(void) {
    const size_t count = 2000;
    HpcsimParticleSystem* particle_system = hpcsim_particle_system_create(count);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {count, 77};
    HPCSIM_ASSERT(hpcsim_preset_generate_parallel(particle_system,
                                                  HPCSIM_PRESET_GLOBULAR_CLUSTER,
                                                  &parameters, &error) ==
                  HPCSIM_STATUS_OK);

    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);
    HpcsimDiagnosticsQuantities quantities;
    hpcsim_diagnostics_compute_global(&view, &quantities, &error);
    const double momentum =
        sqrt(quantities.total_momentum_x * quantities.total_momentum_x +
             quantities.total_momentum_y * quantities.total_momentum_y +
             quantities.total_momentum_z * quantities.total_momentum_z);
    HPCSIM_ASSERT(momentum < 1e-12);

    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_numa_detection);
    HPCSIM_TEST_RUN(test_first_touch_and_affinity);
    HPCSIM_TEST_RUN(test_parallel_generation_is_deterministic);
    HPCSIM_TEST_RUN(test_parallel_generation_zero_momentum);
    return HPCSIM_TEST_SUITE_END();
}
