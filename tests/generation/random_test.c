#include "n_body_sim_pro/generation/random.h"
#include "test_harness.h"

#include <stdint.h>

static void test_same_seed_is_deterministic(void) {
    NBodySimProRandomGenerator first;
    NBodySimProRandomGenerator second;
    n_body_sim_pro_random_init(&first, 12345);
    n_body_sim_pro_random_init(&second, 12345);
    for (int i = 0; i < 100; ++i) {
        N_BODY_SIM_PRO_ASSERT(n_body_sim_pro_random_next_u64(&first) == n_body_sim_pro_random_next_u64(&second));
    }
}

static void test_different_seeds_differ(void) {
    NBodySimProRandomGenerator first;
    NBodySimProRandomGenerator second;
    n_body_sim_pro_random_init(&first, 1);
    n_body_sim_pro_random_init(&second, 2);
    int differing = 0;
    for (int i = 0; i < 16; ++i) {
        if (n_body_sim_pro_random_next_u64(&first) != n_body_sim_pro_random_next_u64(&second)) {
            ++differing;
        }
    }
    N_BODY_SIM_PRO_ASSERT(differing > 0);
}

static void test_next_double_in_unit_interval(void) {
    NBodySimProRandomGenerator generator;
    n_body_sim_pro_random_init(&generator, 42);
    for (int i = 0; i < 10000; ++i) {
        const double value = n_body_sim_pro_random_next_double(&generator);
        N_BODY_SIM_PRO_ASSERT(value >= 0.0);
        N_BODY_SIM_PRO_ASSERT(value < 1.0);
    }
}

static void test_next_double_range(void) {
    NBodySimProRandomGenerator generator;
    n_body_sim_pro_random_init(&generator, 7);
    for (int i = 0; i < 10000; ++i) {
        const double value = n_body_sim_pro_random_next_double_range(&generator, -5.0, 5.0);
        N_BODY_SIM_PRO_ASSERT(value >= -5.0);
        N_BODY_SIM_PRO_ASSERT(value < 5.0);
    }
}

static void test_gaussian_mean_and_spread(void) {
    NBodySimProRandomGenerator generator;
    n_body_sim_pro_random_init(&generator, 99);
    double sum = 0.0;
    double sum_squared = 0.0;
    const int sample_count = 100000;
    for (int i = 0; i < sample_count; ++i) {
        const double value = n_body_sim_pro_random_next_gaussian(&generator);
        sum += value;
        sum_squared += value * value;
    }
    const double mean = sum / (double)sample_count;
    const double variance = sum_squared / (double)sample_count - mean * mean;
    /* mean near 0, variance near 1 for a standard normal */
    N_BODY_SIM_PRO_ASSERT(mean > -0.05 && mean < 0.05);
    N_BODY_SIM_PRO_ASSERT(variance > 0.9 && variance < 1.1);
}

int main(void) {
    N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
    N_BODY_SIM_PRO_TEST_RUN(test_same_seed_is_deterministic);
    N_BODY_SIM_PRO_TEST_RUN(test_different_seeds_differ);
    N_BODY_SIM_PRO_TEST_RUN(test_next_double_in_unit_interval);
    N_BODY_SIM_PRO_TEST_RUN(test_next_double_range);
    N_BODY_SIM_PRO_TEST_RUN(test_gaussian_mean_and_spread);
    return N_BODY_SIM_PRO_TEST_SUITE_END();
}
