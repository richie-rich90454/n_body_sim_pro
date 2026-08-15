#pragma once

#include <n_body_sim_pro/n_body_sim_pro.h>

#include <cstddef>
#include <cstdint>

namespace n_body_sim_pro::benchmark {

/*
 * Headless simulation runner.
 *
 * Runs a simulation without any graphics (SDL/OpenGL/ImGui are never
 * touched) and reports step timing and conservation diagnostics. Used by the
 * `benchmark` and `run` CLI commands so measurement reflects simulation
 * performance rather than rendering.
 */

struct HeadlessOptions {
    NBodySimProSimulationPreset preset = N_BODY_SIM_PRO_PRESET_GALAXY_COLLISION;
    std::size_t particle_count = 65536;
    std::uint64_t random_seed = 42;
    int steps = 100;
    int threads = 0; /* 0 = auto */
    bool barnes_hut = true;
    double theta = 0.7;
    double timestep = 0.001;
};

struct HeadlessReport {
    double average_step_ms = 0.0;
    double first_step_ms = 0.0;
    double tree_build_ms = 0.0;
    double force_evaluation_ms = 0.0;
    double energy_drift = 0.0;
    double momentum_error = 0.0;
    bool energy_available = false;
};

/* Runs the simulation headless; returns 0 on success. */
int run_headless(const HeadlessOptions& options, HeadlessReport& report);

/* Loads a checkpoint and continues it headless for `steps` timesteps. */
int resume_checkpoint(const char* path, int steps, int threads, HeadlessReport& report);

/* Prints the report in human-readable and JSON forms. */
void print_report(const HeadlessOptions& options, const HeadlessReport& report);

}  // namespace n_body_sim_pro::benchmark
