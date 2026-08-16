#include "n_body_sim_pro/barnes_hut/barnes_hut.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/physics/gravity.h"
#include "n_body_sim_pro/simd/backend.h"
#include "n_body_sim_pro/simd/cpu.h"
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

static double maximum_relative_error(NBodySimProParticleSystemView* view,
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
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx2) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 7);
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
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_avx2(&view, &gravity, NULL, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-10);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_avx512_matches_reference_with_softening(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx512_foundation) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 7);
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
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_avx512(&view, &gravity, NULL, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-10);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_neon_matches_reference_with_softening(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_neon) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 7);
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
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_neon(&view, &gravity, NULL, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-10);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_avx2_matches_reference_without_softening(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx2) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 7);
    N_BODY_SIM_PRO_ASSERT(particle_system != NULL);
    if (particle_system == NULL) {
        return;
    }
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.0);

    double reference[3 * TEST_PARTICLE_COUNT];
    n_body_sim_pro_gravity_compute_acceleration_reference(&view, &gravity, NULL, &error);
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_avx2(&view, &gravity, NULL, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-10);

    /* Without softening the self-lane guard must still produce finite values. */
    N_BODY_SIM_PRO_ASSERT(!isnan(view.accelerations_x[0]));
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_openmp_avx2_matches_reference(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx2) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 7);
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
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_gravity_compute_acceleration_openmp_avx2(&view, &gravity, NULL, &error);
    const double relative_error = maximum_relative_error(&view, reference, particle_count);
    N_BODY_SIM_PRO_ASSERT(relative_error < 1.0e-10);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_backend_selection(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    const NBodySimProSimdBackend backend = n_body_sim_pro_simd_best_available_backend(&features);
    N_BODY_SIM_PRO_ASSERT(backend == N_BODY_SIM_PRO_SIMD_BACKEND_SCALAR ||
                  backend == N_BODY_SIM_PRO_SIMD_BACKEND_AVX2 ||
                  backend == N_BODY_SIM_PRO_SIMD_BACKEND_AVX512 ||
                  backend == N_BODY_SIM_PRO_SIMD_BACKEND_NEON);
    N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_simd_backend_string(backend) != NULL);
#ifdef N_BODY_SIM_PRO_HAVE_AVX512_KERNEL
    if (features.has_avx512_foundation && features.has_fma) {
        N_BODY_SIM_PRO_ASSERT(backend == N_BODY_SIM_PRO_SIMD_BACKEND_AVX512);
    }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_AVX2_KERNEL
    if (features.has_avx2 && features.has_fma) {
        N_BODY_SIM_PRO_ASSERT(backend == N_BODY_SIM_PRO_SIMD_BACKEND_AVX2);
    }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_NEON_KERNEL
    if (features.has_neon) {
        N_BODY_SIM_PRO_ASSERT(backend == N_BODY_SIM_PRO_SIMD_BACKEND_NEON);
    }
#endif
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

static void test_barnes_hut_avx2_matches_scalar_barnes_hut(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx2) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 11);
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

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }
    /* The batched SIMD traversal mirrors the scalar traversal's acceptance
     * decisions exactly, so results differ only by floating-point summation
     * order. */
    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.7);

    /* Scalar Barnes-Hut reference. */
    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    double reference[3 * TEST_PARTICLE_COUNT];
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    /* AVX2 Barnes-Hut must agree within tolerance. */
    n_body_sim_pro_barnes_hut_compute_acceleration_avx2(&view, &gravity, tree, &error);
    const double serial_error = root_mean_square_relative_error(&view, reference,
                                                                particle_count);
    N_BODY_SIM_PRO_ASSERT(serial_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx2(&view, &gravity, tree, &error);
    const double parallel_error = root_mean_square_relative_error(&view, reference,
                                                                  particle_count);
    N_BODY_SIM_PRO_ASSERT(parallel_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_barnes_hut_neon_matches_scalar_barnes_hut(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_neon) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 11);
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

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }
    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.7);

    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    double reference[3 * TEST_PARTICLE_COUNT];
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_barnes_hut_compute_acceleration_neon(&view, &gravity, tree, &error);
    const double serial_error = root_mean_square_relative_error(&view, reference,
                                                                particle_count);
    N_BODY_SIM_PRO_ASSERT(serial_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_compute_acceleration_openmp_neon(&view, &gravity, tree, &error);
    const double parallel_error = root_mean_square_relative_error(&view, reference,
                                                                  particle_count);
    N_BODY_SIM_PRO_ASSERT(parallel_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

static void test_barnes_hut_avx512_matches_scalar_barnes_hut(void) {
    NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
    if (!features.has_avx512_foundation) {
        N_BODY_SIM_PRO_ASSERT(1);
        return;
    }

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* particle_system = make_random_system(particle_count, 11);
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

    NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
    N_BODY_SIM_PRO_ASSERT(tree != NULL);
    if (tree == NULL) {
        n_body_sim_pro_particle_system_destroy(particle_system);
        return;
    }
    n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.7);

    n_body_sim_pro_barnes_hut_compute_acceleration(&view, &gravity, tree, &error);
    double reference[3 * TEST_PARTICLE_COUNT];
    for (size_t i = 0; i < particle_count; ++i) {
        reference[3 * i + 0] = view.accelerations_x[i];
        reference[3 * i + 1] = view.accelerations_y[i];
        reference[3 * i + 2] = view.accelerations_z[i];
    }

    n_body_sim_pro_barnes_hut_compute_acceleration_avx512(&view, &gravity, tree, &error);
    const double serial_error = root_mean_square_relative_error(&view, reference,
                                                                particle_count);
    N_BODY_SIM_PRO_ASSERT(serial_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx512(&view, &gravity, tree, &error);
    const double parallel_error = root_mean_square_relative_error(&view, reference,
                                                                  particle_count);
    N_BODY_SIM_PRO_ASSERT(parallel_error < 1.0e-9);

    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    n_body_sim_pro_particle_system_destroy(particle_system);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_avx2_matches_reference_with_softening);
    N_BODY_SIM_PRO_TEST_RUN(test_avx2_matches_reference_without_softening);
    N_BODY_SIM_PRO_TEST_RUN(test_openmp_avx2_matches_reference);
    N_BODY_SIM_PRO_TEST_RUN(test_avx512_matches_reference_with_softening);
    N_BODY_SIM_PRO_TEST_RUN(test_neon_matches_reference_with_softening);
    N_BODY_SIM_PRO_TEST_RUN(test_barnes_hut_avx2_matches_scalar_barnes_hut);
    N_BODY_SIM_PRO_TEST_RUN(test_barnes_hut_avx512_matches_scalar_barnes_hut);
    N_BODY_SIM_PRO_TEST_RUN(test_barnes_hut_neon_matches_scalar_barnes_hut);
    N_BODY_SIM_PRO_TEST_RUN(test_backend_selection);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
