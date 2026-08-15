#include "n_body_sim_pro/barnes_hut/barnes_hut.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/physics/gravity.h"
#include "test_harness.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Barnes-Hut correctness tests.
 *
 * 1. On a two-body system the tree performs exact leaf interactions only, so
 *    the result must be bit-identical to the reference kernel.
 * 2. On a random cloud, Barnes-Hut with a small theta must agree with the
 *    reference within a defined tolerance, and a larger theta must be less
 *    accurate (the theta/accuracy trade-off is measurable, not asserted).
 * 3. The tree must be deterministic and must materialize one leaf per
 *    particle with bounded total node count.
 */

enum { TEST_PARTICLE_COUNT = 400 };

static NBodySimProParticleSystem* make_system(NBodySimProSimulationPreset preset,
                                         size_t particle_count, uint64_t seed) {
    NBodySimProParticleSystem* particle_system =
        n_body_sim_pro_particle_system_create(particle_count);
    if (particle_system == NULL) {
        return NULL;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProPresetParameters parameters = {particle_count, seed};
    n_body_sim_pro_preset_generate(particle_system, preset, &parameters, &error);
    return particle_system;
}

static double root_mean_square_relative_error(NBodySimProParticleSystemView* view,
                                              const double reference[3 * TEST_PARTICLE_COUNT],
                                              size_t particle_count) {
    double sum_error_squared = 0.0;
    double sum_reference_squared = 0.0;
    for (size_t i = 0; i < particle_count; ++i) {
        const double candidate[3] = {view->accelerations_x[i], view->accelerations_y[i],
                                     view->accelerations_z[i]};
        for (size_t component = 0; component < 3; ++component) {
            const double delta = candidate[component] - reference[3 * i + component];
            sum_error_squared += delta * delta;
            sum_reference_squared += reference[3 * i + component] *
                                     reference[3 * i + component];
        }
    }
    if (sum_reference_squared == 0.0) {
        return 0.0;
    }
    return sqrt(sum_error_squared / sum_reference_squared);
}

static void test_two_body_barnes_hut_is_exact(void) {
    NBodySimProParticleSystem* particle_system = make_system(N_BODY_SIM_PRO_PRESET_TWO_BODY, 2, 1);
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

    double reference[3 * TEST_PARTICLE_COUNT];
    n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < view.particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }
    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.7);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error) ==
                  N_BODY_SIM_PRO_STATUS_OK);

    const double relative_error = root_mean_square_relative_error(&view, reference,
                                                                   view.particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-15);

    NBodySimProBarnesHutStats stats;
    n_body_sim_pro_barnes_hut_tree_stats(tree, &stats);
    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(stats.leaf_count, 2);
    N_BODY_SIM_PRO_ASSERT(stats.accepted_approximations == 0);
    N_BODY_SIM_PRO_ASSERT(stats.exact_interactions == 2);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_theta_tradeoff_and_accuracy(void) {
    NBodySimProParticleSystem* particle_system = make_system(N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD, TEST_PARTICLE_COUNT, 7);
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

    double reference[3 * TEST_PARTICLE_COUNT];
    n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < view.particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }

    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.3);
    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    const double accurate_error = root_mean_square_relative_error(&view, reference,
                                                                   view.particle_count);
    NBodySimProBarnesHutStats accurate_stats;
    n_body_sim_pro_barnes_hut_tree_stats(tree, &accurate_stats);
    const size_t accurate_work =
        accurate_stats.accepted_approximations + accurate_stats.exact_interactions;

    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 1.2);
    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    const double coarse_error = root_mean_square_relative_error(&view, reference,
                                                                view.particle_count);
    NBodySimProBarnesHutStats coarse_stats;
    n_body_sim_pro_barnes_hut_tree_stats(tree, &coarse_stats);
    const size_t coarse_work =
        coarse_stats.accepted_approximations + coarse_stats.exact_interactions;

    /* The trade-off is measurable: smaller theta is more accurate but does
     * more total work (exact interactions plus finer approximations). */
    N_BODY_SIM_PRO_ASSERT(accurate_error < 0.01);
    N_BODY_SIM_PRO_ASSERT(accurate_error < coarse_error);
    N_BODY_SIM_PRO_ASSERT(accurate_work > coarse_work);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_tree_structure_and_determinism(void) {
    NBodySimProParticleSystem* particle_system = make_system(N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY, TEST_PARTICLE_COUNT, 99);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.05);

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }
    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.7);

    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    double first_run[3 * TEST_PARTICLE_COUNT];
    for (size_t i = 0; i < view.particle_count; ++i) {
        first_run[3 * i + 0] = view.accelerations_x[i];
        first_run[3 * i + 1] = view.accelerations_y[i];
        first_run[3 * i + 2] = view.accelerations_z[i];
    }
    NBodySimProBarnesHutStats stats;
    n_body_sim_pro_barnes_hut_tree_stats(tree, &stats);

    N_BODY_SIM_PRO_ASSERT_EQ_SIZE(stats.leaf_count, TEST_PARTICLE_COUNT);
    N_BODY_SIM_PRO_ASSERT(stats.node_count <= 2 * TEST_PARTICLE_COUNT);
    N_BODY_SIM_PRO_ASSERT(stats.internal_node_count > 0);
    N_BODY_SIM_PRO_ASSERT(stats.maximum_depth >= 2);

    /* A second evaluation must reproduce the same accelerations exactly. */
    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    int identical = 1;
    for (size_t i = 0; i < view.particle_count; ++i) {
        if (view.accelerations_x[i] != first_run[3 * i + 0] ||
            view.accelerations_y[i] != first_run[3 * i + 1] ||
            view.accelerations_z[i] != first_run[3 * i + 2]) {
            identical = 0;
            break;
        }
    }
    N_BODY_SIM_PRO_ASSERT(identical);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_two_body_barnes_hut_is_exact);
    N_BODY_SIM_PRO_TEST_RUN(test_theta_tradeoff_and_accuracy);
    N_BODY_SIM_PRO_TEST_RUN(test_tree_structure_and_determinism);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
