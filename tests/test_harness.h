#ifndef N_BODY_SIM_PRO_TEST_HARNESS_H
#define N_BODY_SIM_PRO_TEST_HARNESS_H

/*
 * Minimal dependency-free C test harness.
 *
 * Each test file is its own executable registered with CTest. A test file
 * declares free functions, runs them through N_BODY_SIM_PRO_TEST_RUN, and returns
 * the exit code from N_BODY_SIM_PRO_TEST_SUITE_END.
 *
 * Example:
 *
 *   #include "test_harness.h"
 *
 *   static void test_something(void) {
 *       N_BODY_SIM_PRO_ASSERT(1 == 1);
 *       N_BODY_SIM_PRO_ASSERT_NEAR(2.0, 2.0 + 1e-12, 1e-9);
 *   }
 *
 *   int main(void) {
 *       N_BODY_SIM_PRO_TEST_SUITE_BEGIN();
 *       N_BODY_SIM_PRO_TEST_RUN(test_something);
 *       return N_BODY_SIM_PRO_TEST_SUITE_END();
 *   }
 */

#include <math.h>
#include <stdio.h>

typedef struct NBodySimProTestSuite {
    const char* name;
    int total_assertions;
    int failed_assertions;
    int active_test_failures;
} NBodySimProTestSuite;

static NBodySimProTestSuite g_test_suite;

#define N_BODY_SIM_PRO_TEST_SUITE_BEGIN()                                     \
    do {                                                              \
        g_test_suite.name = __FILE__;                                 \
        g_test_suite.total_assertions = 0;                            \
        g_test_suite.failed_assertions = 0;                           \
        g_test_suite.active_test_failures = 0;                        \
        printf("=== %s ===\n", g_test_suite.name);                    \
    } while (0)

#define N_BODY_SIM_PRO_TEST_RUN(test_function)                                \
    do {                                                              \
        g_test_suite.active_test_failures = 0;                        \
        test_function();                                              \
        if (g_test_suite.active_test_failures == 0) {                 \
            printf("[PASS] %s\n", #test_function);                    \
        } else {                                                      \
            printf("[FAIL] %s\n", #test_function);                    \
        }                                                             \
    } while (0)

#define N_BODY_SIM_PRO_ASSERT(condition)                                      \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        if (!(condition)) {                                           \
            printf("  assertion failed at %s:%d: %s\n", __FILE__,     \
                   __LINE__, #condition);                             \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define N_BODY_SIM_PRO_ASSERT_NEAR(actual, expected, tolerance)               \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        const double n_body_sim_pro_actual = (double)(actual);                \
        const double n_body_sim_pro_expected = (double)(expected);            \
        const double n_body_sim_pro_tolerance = (double)(tolerance);          \
        if (!(fabs(n_body_sim_pro_actual - n_body_sim_pro_expected) <= n_body_sim_pro_tolerance)) { \
            printf("  assertion failed at %s:%d: %s (%g) vs %s (%g)"  \
                   " tolerance %g\n", __FILE__, __LINE__, #actual,    \
                   n_body_sim_pro_actual, #expected, n_body_sim_pro_expected,         \
                   n_body_sim_pro_tolerance);                                 \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define N_BODY_SIM_PRO_ASSERT_EQ_SIZE(actual, expected)                       \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        const size_t n_body_sim_pro_actual = (size_t)(actual);                \
        const size_t n_body_sim_pro_expected = (size_t)(expected);            \
        if (n_body_sim_pro_actual != n_body_sim_pro_expected) {                       \
            printf("  assertion failed at %s:%d: %s (%llu) != %s"     \
                   " (%llu)\n", __FILE__, __LINE__, #actual,          \
                   (unsigned long long)n_body_sim_pro_actual, #expected,      \
                   (unsigned long long)n_body_sim_pro_expected);              \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define N_BODY_SIM_PRO_TEST_SUITE_END()                                       \
    (printf("%d assertions, %d failed\n", g_test_suite.total_assertions, \
            g_test_suite.failed_assertions),                          \
     g_test_suite.failed_assertions == 0 ? 0 : 1)

#endif /* N_BODY_SIM_PRO_TEST_HARNESS_H */
