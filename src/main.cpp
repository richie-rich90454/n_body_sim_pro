#include "application/Application.hpp"
#include "benchmark/HeadlessRunner.hpp"

#include <n_body_sim_pro/n_body_sim_pro.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

void print_hardware() {
    const NBodySimProCpuFeatures cpu = n_body_sim_pro_cpu_detect_features();
    std::printf("CPU            : %s\n", n_body_sim_pro_cpu_brand_string());
    std::printf("Architecture   : %s\n",
#if defined(__x86_64__) || defined(_M_X64)
                "x86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
                "ARM64"
#else
                "unknown"
#endif
    );
    std::printf("OpenMP         : %s (%d threads available)\n",
                n_body_sim_pro_threading_openmp_available() ? "enabled" : "unavailable",
                n_body_sim_pro_threading_available_thread_count());
    std::printf("SIMD           :\n");
    std::printf("  SSE2         : %s\n", cpu.has_sse2 ? "available" : "unavailable");
    std::printf("  AVX2         : %s\n", cpu.has_avx2 ? "available" : "unavailable");
    std::printf("  FMA          : %s\n", cpu.has_fma ? "available" : "unavailable");
    std::printf("  AVX-512      : %s\n",
                cpu.has_avx512_foundation ? "available" : "unavailable");
    std::printf("  NEON         : %s\n", cpu.has_neon ? "available" : "unavailable");
    std::printf("Selected backend : %s\n",
                n_body_sim_pro_simd_backend_string(
                    n_body_sim_pro_simd_best_available_backend(&cpu)));
    NBodySimProNumaTopology topology;
    if (n_body_sim_pro_numa_detect(&topology) == 0) {
        std::printf("NUMA            : %d node%s\n", topology.node_count,
                    topology.node_count == 1 ? "" : "s");
        n_body_sim_pro_numa_topology_print(&topology);
        n_body_sim_pro_numa_topology_destroy(&topology);
    } else {
        std::printf("NUMA            : unavailable\n");
    }
}

void print_usage(const char* program_name) {
    std::printf("N-Body Sim Pro - CPU N-Body Simulation Engine\n");
    std::printf("Usage:\n");
    std::printf("  %s                    launch the interactive application\n", program_name);
    std::printf("  %s hardware           print detected CPU/SIMD/OpenMP information\n",
                program_name);
    std::printf("  %s benchmark [options] run a headless benchmark\n", program_name);
    std::printf("    --particles N       particle count\n");
    std::printf("    --steps S           number of simulated steps\n");
    std::printf("    --threads T         OpenMP thread count (0 = auto)\n");
    std::printf("    --algorithm A       all_pairs | barnes_hut\n");
    std::printf("    --theta T           Barnes-Hut opening angle\n");
    std::printf("    --preset P          two_body, random_cloud, solar_system,\n");
    std::printf("                        open_cluster, globular_cluster, spiral_galaxy,\n");
    std::printf("                        elliptical_galaxy, galaxy_collision,\n");
    std::printf("                        triple_galaxy\n");
    std::printf("  %s distributed [--particles N] [--steps S] [--theta T]\n", program_name);
    std::printf("                        distributed run (launch with mpiexec -n P)\n");
    std::printf("  %s resume FILE [--steps S] [--threads T]\n", program_name);
    std::printf("                        continue a checkpoint headless\n");
    std::printf("  %s save FILE [--particles N] [--preset P] [--seed S]\n", program_name);
    std::printf("                        write a fresh preset as a checkpoint\n");
}

bool parse_unsigned(const char* text, unsigned long long* value) {
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char* end = nullptr;
    *value = std::strtoull(text, &end, 10);
    return end != text && *end == '\0';
}

int run_benchmark_command(int argc, char** argv) {
    n_body_sim_pro::benchmark::HeadlessOptions options;
    bool barnes_hut_override = false;

    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        auto next_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "benchmark: missing value for %s\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (argument == "--particles") {
            unsigned long long value;
            if (!parse_unsigned(next_value("--particles"), &value)) {
                return 1;
            }
            options.particle_count = (std::size_t)value;
        } else if (argument == "--steps") {
            unsigned long long value;
            if (!parse_unsigned(next_value("--steps"), &value)) {
                return 1;
            }
            options.steps = (int)value;
        } else if (argument == "--threads") {
            unsigned long long value;
            if (!parse_unsigned(next_value("--threads"), &value)) {
                return 1;
            }
            options.threads = (int)value;
        } else if (argument == "--theta") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "benchmark: missing value for --theta\n");
                return 1;
            }
            options.theta = std::atof(argv[++i]);
        } else if (argument == "--algorithm") {
            const char* value = next_value("--algorithm");
            if (value == nullptr) {
                return 1;
            }
            if (std::strcmp(value, "barnes_hut") == 0) {
                options.barnes_hut = true;
                barnes_hut_override = true;
            } else if (std::strcmp(value, "all_pairs") == 0) {
                options.barnes_hut = false;
                barnes_hut_override = true;
            } else {
                std::fprintf(stderr, "benchmark: unknown algorithm %s\n", value);
                return 1;
            }
        } else if (argument == "--preset") {
            const char* value = next_value("--preset");
            if (value == nullptr) {
                return 1;
            }
            bool found = false;
            for (int preset = 0; preset < N_BODY_SIM_PRO_PRESET_COUNT; ++preset) {
                if (std::strcmp(value, n_body_sim_pro_preset_string((NBodySimProSimulationPreset)preset)) ==
                    0) {
                    options.preset = (NBodySimProSimulationPreset)preset;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::fprintf(stderr, "benchmark: unknown preset %s\n", value);
                return 1;
            }
        } else {
            std::fprintf(stderr, "benchmark: unknown argument %s\n", argv[i]);
            return 1;
        }
    }

    if (!barnes_hut_override) {
        options.barnes_hut = options.particle_count > 20000;
    }

    n_body_sim_pro::benchmark::HeadlessReport report;
    const int status = n_body_sim_pro::benchmark::run_headless(options, report);
    if (status == 0) {
        n_body_sim_pro::benchmark::print_report(options, report);
    }
    return status;
}

}  // namespace

int run_distributed_command(int argc, char** argv) {
    std::size_t particle_count = 65536;
    int steps = 5;
    double theta = 0.7;
    std::uint64_t seed = 42;
    for (int i = 2; i < argc; ++i) {
        const std::string argument = argv[i];
        auto next_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "distributed: missing value for %s\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (argument == "--particles") {
            particle_count = (std::size_t)std::strtoull(next_value("--particles"), nullptr, 10);
        } else if (argument == "--steps") {
            steps = std::atoi(next_value("--steps"));
        } else if (argument == "--theta") {
            theta = std::atof(next_value("--theta"));
        } else if (argument == "--seed") {
            seed = (std::uint64_t)std::strtoull(next_value("--seed"), nullptr, 10);
        } else {
            std::fprintf(stderr, "distributed: unknown argument %s\n", argv[i]);
            return 1;
        }
    }

    NBodySimProMpiRuntime runtime;
    if (n_body_sim_pro_mpi_initialize(&argc, &argv, &runtime) != 0 || !runtime.available) {
        std::fprintf(stderr, "distributed: not running under mpiexec (launch with "
                             "mpiexec -n P %s distributed ...)\n",
                     argv[0]);
        return 1;
    }
    if (runtime.comm_size < 1) {
        n_body_sim_pro_mpi_finalize();
        return 1;
    }

    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    const std::size_t local_count = particle_count / (std::size_t)runtime.comm_size;

    NBodySimProParticleSystem* local = n_body_sim_pro_particle_system_create(local_count);
    if (local == NULL) {
        std::fprintf(stderr, "distributed: failed to allocate local particles\n");
        n_body_sim_pro_mpi_finalize();
        return 1;
    }
    NBodySimProPresetParameters parameters = {local_count,
                                         seed + 0x9E3779B9u * (std::uint64_t)runtime.rank};
    if (n_body_sim_pro_preset_generate_parallel(local, N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD, &parameters,
                                        &error) != N_BODY_SIM_PRO_STATUS_OK) {
        std::fprintf(stderr, "distributed: preset generation failed\n");
        n_body_sim_pro_particle_system_destroy(local);
        n_body_sim_pro_mpi_finalize();
        return 1;
    }

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.02);
    NBodySimProDistributedSimulation* simulation = n_body_sim_pro_distributed_create(&runtime, &error);
    if (simulation == NULL) {
        n_body_sim_pro_particle_system_destroy(local);
        n_body_sim_pro_mpi_finalize();
        return 1;
    }
    n_body_sim_pro_distributed_set_theta(simulation, theta);

    NBodySimProCpuFeatures cpu_features = n_body_sim_pro_cpu_detect_features();
    const NBodySimProSimdBackend simd_backend =
        n_body_sim_pro_simd_best_available_backend(&cpu_features);
    NBodySimProForceFunction distributed_force = n_body_sim_pro_distributed_compute_acceleration;
    switch (simd_backend) {
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX512:
            distributed_force = n_body_sim_pro_distributed_compute_acceleration_avx512;
            break;
        case N_BODY_SIM_PRO_SIMD_BACKEND_NEON:
            distributed_force = n_body_sim_pro_distributed_compute_acceleration_neon;
            break;
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX2:
            distributed_force = n_body_sim_pro_distributed_compute_acceleration_avx2;
            break;
        default:
            break;
    }

    NBodySimProParticleSystemView view;
    n_body_sim_pro_particle_system_view(local, &view, &error);

    const double t0 = n_body_sim_pro_mpi_wall_time();
    for (int step = 0; step < steps; ++step) {
        if (distributed_force(&view, &gravity, simulation, &error) !=
            N_BODY_SIM_PRO_STATUS_OK) {
            std::fprintf(stderr, "distributed: force evaluation failed\n");
            break;
        }
    }
    const double total = n_body_sim_pro_mpi_wall_time() - t0;

    NBodySimProDistributedStats stats;
    n_body_sim_pro_distributed_stats(simulation, &stats);
    std::printf("Rank %d: particles=%zu remote_cells=%zu essential=%zu levels=%d "
                "simd=%s compute=%.2f%% communication=%.2f%% avg_step=%.3f ms\n",
                stats.rank, stats.local_particles, stats.remote_cells,
                stats.essential_cells, stats.levels_exchanged,
                n_body_sim_pro_simd_backend_string(simd_backend),
                total > 0.0 ? 100.0 * stats.computation_time_seconds / total : 0.0,
                total > 0.0 ? 100.0 * stats.communication_time_seconds / total : 0.0,
                1000.0 * total / (double)steps);

    n_body_sim_pro_distributed_destroy(simulation);
    n_body_sim_pro_particle_system_destroy(local);
    n_body_sim_pro_mpi_finalize();
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::strcmp(argv[1], "hardware") == 0) {
            print_hardware();
            return 0;
        }
        if (argc >= 2 && std::strcmp(argv[1], "distributed") == 0) {
            return run_distributed_command(argc, argv);
        }
        if (argc >= 2 && std::strcmp(argv[1], "benchmark") == 0) {
            return run_benchmark_command(argc, argv);
        }
    if (argc >= 2 && std::strcmp(argv[1], "resume") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "resume: missing checkpoint path\n");
            return 1;
        }
        int steps = 100;
        int threads = 0;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
                steps = std::atoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
                threads = std::atoi(argv[++i]);
            } else {
                std::fprintf(stderr, "resume: unknown argument %s\n", argv[i]);
                return 1;
            }
        }
        n_body_sim_pro::benchmark::HeadlessReport report;
        const int status = n_body_sim_pro::benchmark::resume_checkpoint(argv[2], steps, threads,
                                                                report);
        if (status == 0) {
            std::printf("Resumed %zu steps from %s\n", (size_t)steps, argv[2]);
            std::printf("Avg step : %.3f ms\n", report.average_step_ms);
            std::printf("Energy drift : %.6e\n", report.energy_drift);
            std::printf("Momentum error : %.6e\n", report.momentum_error);
        }
        return status;
    }
    if (argc >= 2 && std::strcmp(argv[1], "save") == 0) {
        if (argc < 3) {
            std::fprintf(stderr, "save: missing checkpoint path\n");
            return 1;
        }
        const char* path = argv[2];
        std::size_t particle_count = 65536;
        std::uint64_t seed = 42;
        NBodySimProSimulationPreset preset = N_BODY_SIM_PRO_PRESET_GALAXY_COLLISION;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--particles") == 0 && i + 1 < argc) {
                particle_count = (std::size_t)std::strtoull(argv[++i], nullptr, 10);
            } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
                seed = (std::uint64_t)std::strtoull(argv[++i], nullptr, 10);
            } else if (std::strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
                const char* value = argv[++i];
                bool found = false;
                for (int p = 0; p < N_BODY_SIM_PRO_PRESET_COUNT; ++p) {
                    if (std::strcmp(value,
                                    n_body_sim_pro_preset_string((NBodySimProSimulationPreset)p)) == 0) {
                        preset = (NBodySimProSimulationPreset)p;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::fprintf(stderr, "save: unknown preset %s\n", value);
                    return 1;
                }
            } else {
                std::fprintf(stderr, "save: unknown argument %s\n", argv[i]);
                return 1;
            }
        }

        n_body_sim_pro::SimulationController simulation;
        try {
            simulation.apply_preset(preset, particle_count, seed);
            simulation.save_checkpoint(path);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "save: %s\n", error.what());
            return 1;
        }
        return 0;
    }
    if (argc >= 2 && (std::strcmp(argv[1], "--help") == 0 ||
                      std::strcmp(argv[1], "-h") == 0)) {
        print_usage(argv[0]);
        return 0;
    }
        n_body_sim_pro::application::Application application;
        return application.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "N-Body Sim Pro: fatal error: %s\n", error.what());
        return 1;
    }
}
