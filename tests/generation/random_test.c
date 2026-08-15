#include "hpcsim/generation/random.h"
#include "test_harness.h"

#include <stdint.h>

static void test_same_seed_is_deterministic(void) {
    HpcsimRandomGenerator first;
    HpcsimRandomGenerator second;
    hpcsim_random_init(&first, 12345);
    hpcsim_random_init(&second, 12345);
    for (int i = 0; i < 100; ++i) {
        HPCSIM_ASSERT(hpcsim_random_next_u64(&first) == hpcsim_random_next_u64(&second));
    }
}

static void test_different_seeds_differ(void) {
    HpcsimRandomGenerator first;
    HpcsimRandomGenerator second;
    hpcsim_random_init(&first, 1);
    hpcsim_random_init(&second, 2);
    int differing = 0;
    for (int i = 0; i < 16; ++i) {
        if (hpcsim_random_next_u64(&first) != hpcsim_random_next_u64(&second)) {
            ++differing;
        }
    }
    HPCSIM_ASSERT(differing > 0);
}

static void test_next_double_in_unit_interval(void) {
    HpcsimRandomGenerator generator;
    hpcsim_random_init(&generator, 42);
    for (int i = 0; i < 10000; ++i) {
        const double value = hpcsim_random_next_double(&generator);
        HPCSIM_ASSERT(value >= 0.0);
        HPCSIM_ASSERT(value < 1.0);
    }
}

static void test_next_double_range(void) {
    HpcsimRandomGenerator generator;
    hpcsim_random_init(&generator, 7);
    for (int i = 0; i < 10000; ++i) {
        const double value = hpcsim_random_next_double_range(&generator, -5.0, 5.0);
        HPCSIM_ASSERT(value >= -5.0);
        HPCSIM_ASSERT(value < 5.0);
    }
}

static void test_gaussian_mean_and_spread(void) {
    HpcsimRandomGenerator generator;
    hpcsim_random_init(&generator, 99);
    double sum = 0.0;
    double sum_squared = 0.0;
    const int sample_count = 100000;
    for (int i = 0; i < sample_count; ++i) {
        const double value = hpcsim_random_next_gaussian(&generator);
        sum += value;
        sum_squared += value * value;
    }
    const double mean = sum / (double)sample_count;
    const double variance = sum_squared / (double)sample_count - mean * mean;
    /* mean near 0, variance near 1 for a standard normal */
    HPCSIM_ASSERT(mean > -0.05 && mean < 0.05);
    HPCSIM_ASSERT(variance > 0.9 && variance < 1.1);
}

int main(void) {
    HPCSIM_TEST_SUITE_BEGIN();
    HPCSIM_TEST_RUN(test_same_seed_is_deterministic);
    HPCSIM_TEST_RUN(test_different_seeds_differ);
    HPCSIM_TEST_RUN(test_next_double_in_unit_interval);
    HPCSIM_TEST_RUN(test_next_double_range);
    HPCSIM_TEST_RUN(test_gaussian_mean_and_spread);
    return HPCSIM_TEST_SUITE_END();
}
