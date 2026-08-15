#include "benchmark/HeadlessRunner.hpp"

#include "simulation/SimulationController.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace hpcsim::benchmark {

namespace {
double format_bytes(double bytes) {
    return bytes / (1024.0 * 1024.0);
}
}  // namespace

int run_headless(const HeadlessOptions& options, HeadlessReport& report) {
    if (options.threads > 0) {
        hpcsim_threading_set_thread_count(options.threads);
    }

    SimulationController simulation;
    simulation.barnes_hut_enabled = options.barnes_hut;
    simulation.barnes_hut_theta = options.theta;
    simulation.timestep = options.timestep;
    simulation.running = false;

    try {
        simulation.apply_preset(options.preset, options.particle_count,
                                options.random_seed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "headless: failed to initialize preset: %s\n", error.what());
        return 1;
    }

    /* Warm up: build the tree and validate the first step. */
    simulation.step();
    report.first_step_ms = simulation.last_step_ms();
    report.tree_build_ms = simulation.last_tree_build_ms();
    report.force_evaluation_ms = simulation.last_force_evaluation_ms();

    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step < options.steps; ++step) {
        try {
            simulation.step();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "headless: simulation step failed: %s\n", error.what());
            return 1;
        }
    }
    const auto finish = std::chrono::steady_clock::now();

    report.average_step_ms =
        std::chrono::duration<double, std::milli>(finish - start).count() /
        (double)options.steps;

    const auto& diagnostics = simulation.numerical_diagnostics();
    simulation.refresh_energy_diagnostics();
    const auto& refreshed = simulation.numerical_diagnostics();
    report.energy_drift = refreshed.energy_drift;
    report.momentum_error = diagnostics.momentum_error;
    report.energy_available = refreshed.energy_available;
    return 0;
}

void print_report(const HeadlessOptions& options, const HeadlessReport& report) {
    const HpcsimCpuFeatures cpu = hpcsim_cpu_detect_features();
    HpcsimMemoryEstimate memory;
    hpcsim_error_clear(nullptr);
    HpcsimError error;
    hpcsim_error_clear(&error);
    hpcsim_memory_estimate_simulation(options.particle_count, options.barnes_hut,
                                      &memory, &error);

    std::printf("--- HPCSim headless benchmark ---\n");
    std::printf("CPU            : %s\n", hpcsim_cpu_brand_string());
    std::printf("Architecture   : %s\n",
#if defined(__x86_64__) || defined(_M_X64)
                "x86-64"
#elif defined(__aarch64__) || defined(_M_ARM64)
                "ARM64"
#else
                "unknown"
#endif
    );
    std::printf("SIMD           : AVX2 %s, AVX-512 %s, NEON %s\n",
                cpu.has_avx2 ? "yes" : "no",
                cpu.has_avx512_foundation ? "yes" : "no", cpu.has_neon ? "yes" : "no");
    std::printf("OpenMP threads : %d\n", hpcsim_threading_thread_count());
    std::printf("Particles      : %zu\n", options.particle_count);
    std::printf("Algorithm      : %s (theta %.2f)\n",
                options.barnes_hut ? "barnes_hut" : "all-pairs", options.theta);
    std::printf("Steps          : %d\n", options.steps);
    std::printf("Estimated mem  : %.2f MiB (particles %.2f, tree %.2f)\n",
                format_bytes((double)memory.total_bytes),
                format_bytes((double)memory.particle_bytes),
                format_bytes((double)memory.tree_bytes));
    std::printf("Step time      : avg %.3f ms, first %.3f ms\n", report.average_step_ms,
                report.first_step_ms);
    if (options.barnes_hut) {
        std::printf("Tree build     : %.3f ms\n", report.tree_build_ms);
        std::printf("Force eval     : %.3f ms\n", report.force_evaluation_ms);
    }
    if (report.energy_available) {
        std::printf("Energy drift   : %.6e\n", report.energy_drift);
    } else {
        std::printf("Energy drift   : N/A\n");
    }
    std::printf("Momentum error : %.6e\n", report.momentum_error);

    const std::string energy_drift_string =
        report.energy_available ? std::to_string(report.energy_drift) : std::string("null");
    std::printf("--- machine-readable ---\n");
    std::printf("{\"cpu\":\"%s\",\"simd\":{\"avx2\":%s,\"avx512\":%s,\"neon\":%s},"
                "\"threads\":%d,\"particles\":%zu,\"algorithm\":\"%s\",\"theta\":%.2f,"
                "\"steps\":%d,\"estimated_bytes\":%zu,\"avg_step_ms\":%.6f,"
                "\"first_step_ms\":%.6f,\"energy_drift\":%s,\"momentum_error\":%.6e}\n",
                hpcsim_cpu_brand_string(), cpu.has_avx2 ? "true" : "false",
                cpu.has_avx512_foundation ? "true" : "false", cpu.has_neon ? "true" : "false",
                hpcsim_threading_thread_count(), options.particle_count,
                options.barnes_hut ? "barnes_hut" : "all-pairs", options.theta,
                options.steps, memory.total_bytes, report.average_step_ms,
                report.first_step_ms, energy_drift_string.c_str(), report.momentum_error);
}

}  // namespace hpcsim::benchmark
