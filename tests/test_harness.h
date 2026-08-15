#ifndef HPCSIM_TEST_HARNESS_H
#define HPCSIM_TEST_HARNESS_H

/*
 * Minimal dependency-free C test harness.
 *
 * Each test file is its own executable registered with CTest. A test file
 * declares free functions, runs them through HPCSIM_TEST_RUN, and returns
 * the exit code from HPCSIM_TEST_SUITE_END.
 *
 * Example:
 *
 *   #include "test_harness.h"
 *
 *   static void test_something(void) {
 *       HPCSIM_ASSERT(1 == 1);
 *       HPCSIM_ASSERT_NEAR(2.0, 2.0 + 1e-12, 1e-9);
 *   }
 *
 *   int main(void) {
 *       HPCSIM_TEST_SUITE_BEGIN();
 *       HPCSIM_TEST_RUN(test_something);
 *       return HPCSIM_TEST_SUITE_END();
 *   }
 */

#include <math.h>
#include <stdio.h>

typedef struct HpcsimTestSuite {
    const char* name;
    int total_assertions;
    int failed_assertions;
    int active_test_failures;
} HpcsimTestSuite;

static HpcsimTestSuite g_test_suite;

#define HPCSIM_TEST_SUITE_BEGIN()                                     \
    do {                                                              \
        g_test_suite.name = __FILE__;                                 \
        g_test_suite.total_assertions = 0;                            \
        g_test_suite.failed_assertions = 0;                           \
        g_test_suite.active_test_failures = 0;                        \
        printf("=== %s ===\n", g_test_suite.name);                    \
    } while (0)

#define HPCSIM_TEST_RUN(test_function)                                \
    do {                                                              \
        g_test_suite.active_test_failures = 0;                        \
        test_function();                                              \
        if (g_test_suite.active_test_failures == 0) {                 \
            printf("[PASS] %s\n", #test_function);                    \
        } else {                                                      \
            printf("[FAIL] %s\n", #test_function);                    \
        }                                                             \
    } while (0)

#define HPCSIM_ASSERT(condition)                                      \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        if (!(condition)) {                                           \
            printf("  assertion failed at %s:%d: %s\n", __FILE__,     \
                   __LINE__, #condition);                             \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define HPCSIM_ASSERT_NEAR(actual, expected, tolerance)               \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        const double hpcsim_actual = (double)(actual);                \
        const double hpcsim_expected = (double)(expected);            \
        const double hpcsim_tolerance = (double)(tolerance);          \
        if (!(fabs(hpcsim_actual - hpcsim_expected) <= hpcsim_tolerance)) { \
            printf("  assertion failed at %s:%d: %s (%g) vs %s (%g)"  \
                   " tolerance %g\n", __FILE__, __LINE__, #actual,    \
                   hpcsim_actual, #expected, hpcsim_expected,         \
                   hpcsim_tolerance);                                 \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define HPCSIM_ASSERT_EQ_SIZE(actual, expected)                       \
    do {                                                              \
        ++g_test_suite.total_assertions;                              \
        const size_t hpcsim_actual = (size_t)(actual);                \
        const size_t hpcsim_expected = (size_t)(expected);            \
        if (hpcsim_actual != hpcsim_expected) {                       \
            printf("  assertion failed at %s:%d: %s (%llu) != %s"     \
                   " (%llu)\n", __FILE__, __LINE__, #actual,          \
                   (unsigned long long)hpcsim_actual, #expected,      \
                   (unsigned long long)hpcsim_expected);              \
            ++g_test_suite.failed_assertions;                         \
            ++g_test_suite.active_test_failures;                      \
        }                                                             \
    } while (0)

#define HPCSIM_TEST_SUITE_END()                                       \
    (printf("%d assertions, %d failed\n", g_test_suite.total_assertions, \
            g_test_suite.failed_assertions),                          \
     g_test_suite.failed_assertions == 0 ? 0 : 1)

#endif /* HPCSIM_TEST_HARNESS_H */
