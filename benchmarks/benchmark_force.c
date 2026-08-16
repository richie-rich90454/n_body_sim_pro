#include "n_body_sim_pro/barnes_hut/barnes_hut.h"
#include "n_body_sim_pro/generation/presets.h"
#include "n_body_sim_pro/physics/gravity.h"
#include "n_body_sim_pro/threading/threading.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Headless force-kernel benchmark.
 *
 * Measures the cost of a single force evaluation for a given particle count,
 * algorithm, and thread count. This isolates the dominant simulation cost
 * from rendering. Every result is real: the force kernel is actually run
 * `steps` times and wall-clock time is measured.
 *
 * Usage:
 *   n_body_sim_pro_benchmark --particles N --steps S --threads T1[,T2,...]
 *
 * Output is human-readable on stdout and a machine-readable CSV line is
 * printed last (useful for regression tracking).
 */

typedef struct BenchmarkOptions {
    size_t particle_count;
    int steps;
    const char* threads_list;
    const char* algorithm;
    double theta;
} BenchmarkOptions;

static double wall_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}

static void print_usage(const char* program_name) {
    fprintf(stderr,
            "Usage: %s --particles N --steps S [--threads T1,T2,..] [--algorithm "
            "reference|openmp|avx2|openmp_avx2|avx512|openmp_avx512|neon|openmp_neon|"
            "barnes_hut|barnes_hut_avx2|barnes_hut_openmp_avx2|barnes_hut_avx512|"
            "barnes_hut_openmp_avx512|barnes_hut_neon|barnes_hut_openmp_neon] [--theta T]\n",
            program_name);
}

static int parse_arguments(int argc, char** argv, BenchmarkOptions* options) {
    memset(options, 0, sizeof(*options));
    options->particle_count = 10000;
    options->steps = 5;
    options->threads_list = "1";
    options->algorithm = "openmp";
    options->theta = 0.7;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--particles") == 0 && i + 1 < argc) {
            options->particle_count = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            options->steps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            options->threads_list = argv[++i];
        } else if (strcmp(argv[i], "--algorithm") == 0 && i + 1 < argc) {
            options->algorithm = argv[++i];
        } else if (strcmp(argv[i], "--theta") == 0 && i + 1 < argc) {
            options->theta = atof(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 0;
        }
    }
    return 1;
}

static double measure_force_evaluation(const NBodySimProGravity* gravity,
                                       NBodySimProParticleSystemView* view, int steps,
                                       const char* algorithm, double theta) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProBarnesHutTree* tree = NULL;
    if (strcmp(algorithm, "barnes_hut") == 0 ||
        strcmp(algorithm, "barnes_hut_avx2") == 0 ||
        strcmp(algorithm, "barnes_hut_openmp_avx2") == 0 ||
        strcmp(algorithm, "barnes_hut_avx512") == 0 ||
        strcmp(algorithm, "barnes_hut_openmp_avx512") == 0 ||
        strcmp(algorithm, "barnes_hut_neon") == 0 ||
        strcmp(algorithm, "barnes_hut_openmp_neon") == 0) {
        tree = n_body_sim_pro_barnes_hut_tree_create(&error);
        if (tree == NULL) {
            fprintf(stderr, "failed to create Barnes-Hut tree\n");
            return -1.0;
        }
        n_body_sim_pro_barnes_hut_tree_set_theta(tree, theta);
    }
    const double start = wall_time_seconds();
    for (int step = 0; step < steps; ++step) {
        NBodySimProStatus status = N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
        if (strcmp(algorithm, "reference") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_reference(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "openmp") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_openmp(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "avx2") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_avx2(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "openmp_avx2") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_openmp_avx2(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "avx512") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_avx512(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "openmp_avx512") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_openmp_avx512(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "neon") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_neon(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "openmp_neon") == 0) {
            status = n_body_sim_pro_gravity_compute_acceleration_openmp_neon(view, gravity, NULL, &error);
        } else if (strcmp(algorithm, "barnes_hut_avx2") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_avx2(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut_openmp_avx2") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx2(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut_avx512") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_avx512(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut_openmp_avx512") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx512(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut_neon") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_neon(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut_openmp_neon") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration_openmp_neon(view, gravity, tree, &error);
        } else if (strcmp(algorithm, "barnes_hut") == 0) {
            status = n_body_sim_pro_barnes_hut_compute_acceleration(view, gravity, tree, &error);
        }
        if (status != N_BODY_SIM_PRO_STATUS_OK) {
            fprintf(stderr, "force evaluation failed: %s\n", n_body_sim_pro_status_string(status));
            n_body_sim_pro_barnes_hut_tree_destroy(tree);
            return -1.0;
        }
    }
    const double elapsed = wall_time_seconds() - start;
    n_body_sim_pro_barnes_hut_tree_destroy(tree);
    return elapsed / (double)steps;
}

int main(int argc, char** argv) {
    BenchmarkOptions options;
    if (!parse_arguments(argc, argv, &options)) {
        return 1;
    }

    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    NBodySimProParticleSystem* particle_system =
        n_body_sim_pro_particle_system_create(options.particle_count);
    if (particle_system == NULL) {
        fprintf(stderr, "failed to allocate particle system\n");
        return 1;
    }
    NBodySimProPresetParameters parameters = {options.particle_count, 42};
    if (n_body_sim_pro_preset_generate(particle_system, N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD, &parameters,
                               &error) != N_BODY_SIM_PRO_STATUS_OK) {
        fprintf(stderr, "failed to generate particles\n");
        n_body_sim_pro_particle_system_destroy(particle_system);
        return 1;
    }

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.01);
    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(particle_system, &view, &error);

    printf("particles=%zu steps=%d algorithm=%s\n", options.particle_count,
           options.steps, options.algorithm);

    double baseline = 0.0;
    char csv_line[1024];
    size_t csv_offset = 0;
    int first = 1;

    const char* token = options.threads_list;
    while (token != NULL && *token != '\0') {
        const char* comma = strchr(token, ',');
        char token_buffer[64];
        size_t token_length =
            comma != NULL ? (size_t)(comma - token) : strlen(token);
        if (token_length >= sizeof(token_buffer)) {
            break;
        }
        memcpy(token_buffer, token, token_length);
        token_buffer[token_length] = '\0';

        const int thread_count = atoi(token_buffer);
        const int uses_threads = strstr(options.algorithm, "openmp") != NULL;
        if (uses_threads) {
            n_body_sim_pro_threading_set_thread_count(thread_count);
        }
        const int active_threads = uses_threads ? n_body_sim_pro_threading_thread_count() : 1;

        const double seconds_per_evaluation = measure_force_evaluation(&gravity, &view, options.steps, options.algorithm, options.theta);
        if (seconds_per_evaluation < 0.0) {
            n_body_sim_pro_particle_system_destroy(particle_system);
            return 1;
        }

        const double milliseconds = seconds_per_evaluation * 1000.0;
        const double speedup = baseline > 0.0 ? baseline / seconds_per_evaluation : 1.0;
        const double parallel_efficiency =
            baseline > 0.0 && active_threads > 1
                ? speedup / (double)active_threads
                : 0.0;

        printf("threads=%-3d ms/evaluation=%-12.3f speedup=%-8.3f efficiency=%.3f\n",
               active_threads, milliseconds, speedup, parallel_efficiency);

        if (first) {
            baseline = seconds_per_evaluation;
            csv_offset = (size_t)snprintf(csv_line, sizeof(csv_line), "threads,ms,"
                                          "speedup,efficiency\n");
            first = 0;
        }
        csv_offset += (size_t)snprintf(csv_line + csv_offset,
                                       sizeof(csv_line) - csv_offset,
                                       "%d,%.6f,%.6f,%.6f\n", active_threads,
                                       milliseconds, speedup, parallel_efficiency);

        token = comma != NULL ? comma + 1 : NULL;
    }

    printf("%s", csv_line);
    n_body_sim_pro_particle_system_destroy(particle_system);
    return 0;
}
