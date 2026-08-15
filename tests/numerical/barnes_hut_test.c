#include "hpcsim/barnes_hut/barnes_hut.h"
#include "hpcsim/generation/presets.h"
#include "hpcsim/physics/gravity.h"
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

static HpcsimParticleSystem* make_system(HpcsimSimulationPreset preset,
                                         size_t particle_count, uint64_t seed) {
    HpcsimParticleSystem* particle_system =
        hpcsim_particle_system_create(particle_count);
    if (particle_system == NULL) {
        return NULL;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimPresetParameters parameters = {particle_count, seed};
    hpcsim_preset_generate(particle_system, preset, &parameters, &error);
    return particle_system;
}

static double root_mean_square_relative_error(HpcsimParticleSystemView* view,
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
    HpcsimParticleSystem* particle_system = make_system(HPCSIM_PRESET_TWO_BODY, 2, 1);
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
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < view.particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    HpcsimBarnesHutTree* tree = hpcsim_barnes_hut_tree_create(&error);
    HPCSIM_ASSERT(tree != NULL);
    if (tree == NULL) {
        hpcsim_particle_system_destroy(particle_system);
        return;
    }
    hpcsim_barnes_hut_tree_set_theta(tree, 0.7);
    HPCSIM_ASSERT(hpcsim_barnes_hut_compute_acceleration(&view, &gravity, tree, &error) ==
                  HPCSIM_STATUS_OK);

    const double relative_error = root_mean_square_relative_error(&view, reference,
                                                                   view.particle_count);
    HPCSIM_ASSERT(relative_error < 1.0e-15);

    HpcsimBarnesHutStats stats;
    hpcsim_barnes_hut_tree_stats(tree, &stats);
    HPCSIM_ASSERT_EQ_SIZE(stats.leaf_count, 2);
    HPCSIM_ASSERT(stats.accepted_approximations == 0);
    HPCSIM_ASSERT(stats.exact_interactions == 2);

    hpcsim_barnes_hut_tree_destroy(tree);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_theta_tradeoff_and_accuracy(void) {
    HpcsimParticleSystem* particle_system = make_system(HPCSIM_PRESET_RANDOM_CLOUD, TEST_PARTICLE_COUNT, 7);
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
    hpcsim_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < view.particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    HpcsimBarnesHutTree* tree = hpcsim_barnes_hut_tree_create(&error);
    HPCSIM_ASSERT(tree != NULL);
    if (tree == NULL) {
        hpcsim_particle_system_destroy(particle_system);
        return;
    }

    hpcsim_barnes_hut_tree_set_theta(tree, 0.3);
    hpcsim_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    const double accurate_error = root_mean_square_relative_error(&view, reference,
                                                                   view.particle_count);
    HpcsimBarnesHutStats accurate_stats;
    hpcsim_barnes_hut_tree_stats(tree, &accurate_stats);
    const size_t accurate_work =
        accurate_stats.accepted_approximations + accurate_stats.exact_interactions;

    hpcsim_barnes_hut_tree_set_theta(tree, 1.2);
    hpcsim_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    const double coarse_error = root_mean_square_relative_error(&view, reference,
                                                                view.particle_count);
    HpcsimBarnesHutStats coarse_stats;
    hpcsim_barnes_hut_tree_stats(tree, &coarse_stats);
    const size_t coarse_work =
        coarse_stats.accepted_approximations + coarse_stats.exact_interactions;

    /* The trade-off is measurable: smaller theta is more accurate but does
     * more total work (exact interactions plus finer approximations). */
    HPCSIM_ASSERT(accurate_error < 0.01);
    HPCSIM_ASSERT(accurate_error < coarse_error);
    HPCSIM_ASSERT(accurate_work > coarse_work);

    hpcsim_barnes_hut_tree_destroy(tree);
    hpcsim_particle_system_destroy(particle_system);
}

static void test_tree_structure_and_determinism(void) {
    HpcsimParticleSystem* particle_system = make_system(HPCSIM_PRESET_SPIRAL_GALAXY, TEST_PARTICLE_COUNT, 99);
    HPCSIM_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimParticleSystemView view;
    hpcsim_particle_system_view(particle_system, &view, &error);

    HpcsimGravity gravity;
    hpcsim_gravity_init(&gravity, 1.0, 0.05);

    HpcsimBarnesHutTree* tree = hpcsim_barnes_hut_tree_create(&error);
    HPCSIM_ASSERT(tree != NULL);
    if (tree == NULL) {
        hpcsim_particle_system_destroy(particle_system);
        return;
    }
    hpcsim_barnes_hut_tree_set_theta(tree, 0.7);

    hpcsim_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    double first_run[3 * TEST_PARTICLE_COUNT];
    for (size_t i = 0; i < view.particle_count; ++i) {
        first_run[3 * i + 0] = view.accelerations_x[i];
        first_run[3 * i + 1] = view.accelerations_y[i];
        first_run[3 * i + 2] = view.accelerations_z[i];
    }
    HpcsimBarnesHutStats stats;
    hpcsim_barnes_hut_tree_stats(tree, &stats);

    HPCSIM_ASSERT_EQ_SIZE(stats.leaf_count, TEST_PARTICLE_COUNT);
    HPCSIM_ASSERT(stats.node_count <= 2 * TEST_PARTICLE_COUNT);
    HPCSIM_ASSERT(stats.internal_node_count > 0);
    HPCSIM_ASSERT(stats.maximum_depth >= 2);

    /* A second evaluation must reproduce the same accelerations exactly. */
    hpcsim_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    int identical = 1;
    for (size_t i = 0; i < view.particle_count; ++i) {
        if (view.accelerations_x[i] != first_run[3 * i + 0] ||
            view.accelerations_y[i] != first_run[3 * i + 1] ||
            view.accelerations_z[i] != first_run[3 * i + 2]) {
            identical = 0;
            break;
        }
    }
    HPCSIM_ASSERT(identical);

    hpcsim_barnes_hut_tree_destroy(tree);
    hpcsim_particle_system_destroy(particle_system);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_two_body_barnes_hut_is_exact);
    HPCSIM_TEST_RUN(test_theta_tradeoff_and_accuracy);
    HPCSIM_TEST_RUN(test_tree_structure_and_determinism);
    return HPCSIM_TEST_SUITE_END();
}
