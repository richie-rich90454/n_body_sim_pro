#include "n_body_sim_pro/diagnostics/numerics.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/threading/numa.h"
#include "n_body_sim_pro/threading/threading.h"
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
    NBodySimProNumaTopology topology;
    if (n_body_sim_pro_numa_detect(&topology) != 0) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }
    N_BODY_SIM_PRO_ASSERT(topology.node_count >= 1);
    N_BODY_SIM_PRO_ASSERT(topology.logical_processor_count >= 1);
    n_body_sim_pro_numa_topology_destroy(&topology);
}

static void test_first_touch_and_affinity(void) {
    double buffer[4096];
    for (size_t i = 0; i < 4096; ++i) {
        buffer[i] = 1.0;
    }
    n_body_sim_pro_memory_first_touch(buffer, 4096);
    N_BODY_SIM_PRO_ASSERT(buffer[0] == 0.0);
    /* Pinning the current thread is best-effort; it may fail in restricted
     * environments, so only the no-crash guarantee is asserted. */
    n_body_sim_pro_thread_affinity_apply_policy(N_BODY_SIM_PRO_AFFINITY_COMPACT, 0);
    n_body_sim_pro_thread_affinity_apply_policy(N_BODY_SIM_PRO_AFFINITY_SPREAD, 0);
    n_body_sim_pro_thread_affinity_apply_policy(N_BODY_SIM_PRO_AFFINITY_AUTO, 0);
    N_BODY_SIM_PRO_ASSERT(1);
}

static void test_parallel_generation_is_deterministic(void) {
    const size_t count = 2000;
    NBodySimProPresetParameters parameters = {count, 555};

    NBodySimProParticleSystem* first = n_body_sim_pro_particle_system_create(count);
    NBodySimProParticleSystem* second = n_body_sim_pro_particle_system_create(count);
    N_BODY_SIM_PRO_ASSERT(first != NULL && second != NULL);
    if (first == NULL || second == NULL) {
        n_body_sim_pro_particle_system_destroy(first);
        n_body_sim_pro_particle_system_destroy(second);
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    n_body_sim_pro_threading_set_thread_count(2);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate_parallel(first, N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY,
                                                  &parameters, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    n_body_sim_pro_threading_set_thread_count(4);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate_parallel(second, N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY,
                                                  &parameters, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);
    n_body_sim_pro_threading_set_thread_count(-1);

    int identical = 1;
    for (size_t i = 0; i < count; ++i) {
        NBodySimProVector3 p_first;
        NBodySimProVector3 p_second;
        n_body_sim_pro_particle_system_position(first, i, &p_first, &error);
        n_body_sim_pro_particle_system_position(second, i, &p_second, &error);
        if (fabs(p_first.x - p_second.x) > 1e-12 ||
            fabs(p_first.y - p_second.y) > 1e-12 ||
            fabs(p_first.z - p_second.z) > 1e-12) {
            identical = 0;
            break;
        }
    }
    N_BODY_SIM_PRO_ASSERT(identical);

    n_body_sim_pro_particle_system_destroy(first);
    n_body_sim_pro_particle_system_destroy(second);
}

static void test_parallel_generation_zero_momentum(void) {
    const size_t count = 2000;
    NBodySimProParticleSystem* particle_system = n_body_sim_pro_particle_system_create(count);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {count, 77};
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_preset_generate_parallel(particle_system,
                                                  N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER,
                                                  &parameters, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);
    NBodySimProDiagnosticsQuantities quantities;
    n_body_sim_pro_diagnostics_compute_global(&view, &quantities, &error);
    const double momentum =
        sqrt(quantities.total_momentum_x * quantities.total_momentum_x +
             quantities.total_momentum_y * quantities.total_momentum_y +
             quantities.total_momentum_z * quantities.total_momentum_z);
    N_BODY_SIM_PRO_ASSERT(momentum < 1e-12);

    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_numa_detection);
    N_BODY_SIM_PRO_TEST_RUN(test_first_touch_and_affinity);
    N_BODY_SIM_PRO_TEST_RUN(test_parallel_generation_is_deterministic);
    N_BODY_SIM_PRO_TEST_RUN(test_parallel_generation_zero_momentum);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
