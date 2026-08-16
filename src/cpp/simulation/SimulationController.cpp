#include "simulation/SimulationController.hpp"

#include "logging/Logger.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace n_body_sim_pro {

SimulationController::SimulationController() : particles_(2) {
    recompute_gravity_parameters();
    for (auto& trail : trails_) {
        trail.reserve(TRAIL_CAPACITY);
    }
    const NBodySimProCpuFeatures cpu_features = n_body_sim_pro_cpu_detect_features();
    simd_backend_ = n_body_sim_pro_simd_best_available_backend(&cpu_features);
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Simd,
               "Selected SIMD backend: %s", n_body_sim_pro_simd_backend_string(simd_backend_));
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Threading, "OpenMP available: %s",
               n_body_sim_pro_threading_openmp_available() ? "yes" : "no");
    apply_preset(N_BODY_SIM_PRO_PRESET_TWO_BODY, 2, 1);
}

SimulationController::~SimulationController() = default;

void SimulationController::apply_preset(NBodySimProSimulationPreset preset,
                                        std::size_t particle_count,
                                        std::uint64_t random_seed) {
    if (particle_count == 0) {
        throw std::runtime_error("SimulationController: preset particle count is zero");
    }
    ParticleSystem replacement(particle_count);

    NBodySimProPresetParameters parameters;
    parameters.particle_count = particle_count;
    parameters.random_seed = random_seed;

    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProStatus status;
    if (use_parallel_generation) {
        status = n_body_sim_pro_preset_generate_parallel(replacement.handle(), preset, &parameters,
                                                 &error);
    } else {
        status = n_body_sim_pro_preset_generate(replacement.handle(), preset, &parameters, &error);
    }
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: preset generation "
                                             "failed: ") +
                                 n_body_sim_pro_status_string(status));
    }

    particles_ = std::move(replacement);
    preset_ = preset;
    random_seed_ = random_seed;
    simulation_time = 0.0;
    clear_trails();
    compute_initial_accelerations();
    reset_diagnostics_reference();
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Simulation,
               "Initialized preset %s with %zu particles (seed %llu)",
               n_body_sim_pro_preset_string(preset), particle_count,
               (unsigned long long)random_seed);
}

NBodySimProBarnesHutTree* SimulationController::barnes_hut_tree() {
    if (!tree_) {
        NBodySimProError error;
        n_body_sim_pro_error_clear(&error);
        tree_.reset(n_body_sim_pro_barnes_hut_tree_create(&error));
        if (!tree_) {
            throw std::runtime_error("SimulationController: failed to create Barnes-Hut "
                                     "tree context");
        }
    }
    n_body_sim_pro_barnes_hut_tree_set_theta(tree_.get(), barnes_hut_theta);
    return tree_.get();
}

NBodySimProForceFunction SimulationController::select_force_function(void*& force_context) const {
    if (barnes_hut_enabled) {
        force_context = const_cast<NBodySimProBarnesHutTree*>(tree_.get());
        if (use_simd_barnes_hut) {
            switch (simd_backend_) {
                case N_BODY_SIM_PRO_SIMD_BACKEND_AVX512:
                    return n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx512;
                case N_BODY_SIM_PRO_SIMD_BACKEND_NEON:
                    return n_body_sim_pro_barnes_hut_compute_acceleration_openmp_neon;
                case N_BODY_SIM_PRO_SIMD_BACKEND_AVX2:
                    return n_body_sim_pro_barnes_hut_compute_acceleration_openmp_avx2;
                default:
                    break;
            }
        }
        return n_body_sim_pro_barnes_hut_compute_acceleration;
    }
    force_context = nullptr;
    const bool openmp_available = n_body_sim_pro_threading_openmp_available() != 0;
    if (use_parallel_forces && openmp_available) {
        switch (simd_backend_) {
            case N_BODY_SIM_PRO_SIMD_BACKEND_AVX512:
                return n_body_sim_pro_gravity_compute_acceleration_openmp_avx512;
            case N_BODY_SIM_PRO_SIMD_BACKEND_NEON:
                return n_body_sim_pro_gravity_compute_acceleration_openmp_neon;
            case N_BODY_SIM_PRO_SIMD_BACKEND_AVX2:
                return n_body_sim_pro_gravity_compute_acceleration_openmp_avx2;
            default:
                return n_body_sim_pro_gravity_compute_acceleration_openmp;
        }
    }
    switch (simd_backend_) {
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX512:
            return n_body_sim_pro_gravity_compute_acceleration_avx512;
        case N_BODY_SIM_PRO_SIMD_BACKEND_NEON:
            return n_body_sim_pro_gravity_compute_acceleration_neon;
        case N_BODY_SIM_PRO_SIMD_BACKEND_AVX2:
            return n_body_sim_pro_gravity_compute_acceleration_avx2;
        default:
            return n_body_sim_pro_gravity_compute_acceleration_reference;
    }
}

void SimulationController::compute_initial_accelerations() {
    if (barnes_hut_enabled) {
        n_body_sim_pro_barnes_hut_tree_set_theta(barnes_hut_tree(), barnes_hut_theta);
    }
    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    void* force_context = nullptr;
    NBodySimProForceFunction force_function = select_force_function(force_context);
    NBodySimProStatus status = force_function(&view, &gravity_, force_context, &error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error("SimulationController: failed to compute initial forces");
    }
}

void SimulationController::recompute_gravity_parameters() {
    n_body_sim_pro_gravity_init(&gravity_, gravitational_constant_, softening_length_);
}

void SimulationController::set_gravitational_constant(double value) {
    gravitational_constant_ = value;
    recompute_gravity_parameters();
}

void SimulationController::set_softening_length(double value) {
    softening_length_ = value;
    recompute_gravity_parameters();
}

void SimulationController::step() {
    if (barnes_hut_enabled) {
        n_body_sim_pro_barnes_hut_tree_set_theta(barnes_hut_tree(), barnes_hut_theta);
    }
    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    void* force_context = nullptr;
    NBodySimProForceFunction force_function = select_force_function(force_context);

    const auto step_start = std::chrono::steady_clock::now();
    NBodySimProStatus status = n_body_sim_pro_integrator_advance(&view, &gravity_, integrator, timestep,
                                                    force_function, force_context, &error);
    const auto step_end = std::chrono::steady_clock::now();
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error("SimulationController: integrator step failed");
    }

    last_step_ms_ = std::chrono::duration<double, std::milli>(step_end - step_start).count();
    if (barnes_hut_enabled) {
        NBodySimProBarnesHutStats stats;
        if (n_body_sim_pro_barnes_hut_tree_stats(tree_.get(), &stats)) {
            last_tree_build_ms_ = 0.0;
            last_force_evaluation_ms_ = 0.0;
        } else {
            last_tree_build_ms_ = stats.build_time_seconds * 1000.0;
            last_force_evaluation_ms_ = stats.evaluation_time_seconds * 1000.0;
        }
    } else {
        last_tree_build_ms_ = 0.0;
        last_force_evaluation_ms_ = last_step_ms_;
    }

    simulation_time += timestep;
    refresh_numerical_diagnostics();
    if (preset_ == N_BODY_SIM_PRO_PRESET_TWO_BODY) {
        record_trail_positions();
    }
}

void SimulationController::refresh_energy_diagnostics() {
    if (!diagnostics_.energy_available) {
        return;
    }
    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProDiagnosticsQuantities quantities;
    n_body_sim_pro_diagnostics_compute_global(&view, &quantities, &error);
    double potential_energy = 0.0;
    if (n_body_sim_pro_diagnostics_compute_potential_energy(&view, &gravity_, &potential_energy,
                                                    &error) == N_BODY_SIM_PRO_STATUS_OK) {
        const double total_energy = quantities.kinetic_energy + potential_energy;
        const double reference_energy =
            std::fabs(initial_total_energy_) > 0.0 ? std::fabs(initial_total_energy_) : 1.0;
        diagnostics_.energy_drift =
            std::fabs(total_energy - initial_total_energy_) / reference_energy;
    }
}

void SimulationController::save_checkpoint(const char* path) {
    NBodySimProCheckpointHeader header{};
    header.magic = N_BODY_SIM_PRO_CHECKPOINT_MAGIC;
    header.version = N_BODY_SIM_PRO_CHECKPOINT_VERSION;
    header.particle_count = particles_.particle_count();
    header.simulation_time = simulation_time;
    header.timestep = timestep;
    header.integrator = static_cast<int32_t>(integrator);
    header.theta = barnes_hut_theta;
    header.barnes_hut_enabled = barnes_hut_enabled ? 1 : 0;
    header.random_seed = random_seed_;
    header.preset = static_cast<int32_t>(preset_);

    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    const NBodySimProStatus status =
        n_body_sim_pro_checkpoint_write(path, &view, &header, &error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint write "
                                             "failed: ") +
                                 n_body_sim_pro_status_string(status));
    }
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Simulation,
               "Checkpoint saved to %s (%zu particles, t=%.4f)", path,
               header.particle_count, simulation_time);
}

void SimulationController::load_checkpoint(const char* path) {
    NBodySimProCheckpointHeader header{};
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProStatus status = n_body_sim_pro_checkpoint_peek(path, &header, &error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint peek "
                                             "failed: ") +
                                 n_body_sim_pro_status_string(status));
    }
    ParticleSystem replacement(header.particle_count);
    n_body_sim_pro_error_clear(&error);
    status = n_body_sim_pro_checkpoint_read(path, &header, replacement.handle(), &error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint read "
                                             "failed: ") +
                                 n_body_sim_pro_status_string(status));
    }

    particles_ = std::move(replacement);
    simulation_time = header.simulation_time;
    timestep = header.timestep;
    integrator = static_cast<NBodySimProIntegratorType>(header.integrator);
    barnes_hut_theta = header.theta;
    barnes_hut_enabled = header.barnes_hut_enabled != 0;
    random_seed_ = header.random_seed;
    preset_ = static_cast<NBodySimProSimulationPreset>(header.preset);

    clear_trails();
    reset_diagnostics_reference();
    N_BODY_SIM_PRO_LOG(logging::Level::Info, logging::Category::Simulation,
               "Checkpoint loaded from %s (%zu particles, t=%.4f)", path,
               header.particle_count, simulation_time);
}

void SimulationController::reset_diagnostics_reference() {
    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProDiagnosticsQuantities quantities;
    n_body_sim_pro_diagnostics_compute_global(&view, &quantities, &error);

    initial_momentum_ = {quantities.total_momentum_x, quantities.total_momentum_y,
                         quantities.total_momentum_z};
    initial_center_of_mass_ = {quantities.center_of_mass_x, quantities.center_of_mass_y,
                               quantities.center_of_mass_z};
    momentum_scale_ = 1.0;
    const double* const velocities_x = view.velocities_x;
    const double* const velocities_y = view.velocities_y;
    const double* const velocities_z = view.velocities_z;
    const double* const masses = view.masses;
    for (std::size_t i = 0; i < view.particle_count; ++i) {
        momentum_scale_ +=
            masses[i] * std::sqrt(velocities_x[i] * velocities_x[i] +
                                  velocities_y[i] * velocities_y[i] +
                                  velocities_z[i] * velocities_z[i]);
    }
    if (momentum_scale_ == 0.0) {
        momentum_scale_ = 1.0;
    }

    diagnostics_ = {};
    diagnostics_.energy_available = false;
    if (view.particle_count <= ENERGY_TRACK_MAX_PARTICLES) {
        double potential_energy = 0.0;
        if (n_body_sim_pro_diagnostics_compute_potential_energy(&view, &gravity_,
                                                        &potential_energy,
                                                        &error) == N_BODY_SIM_PRO_STATUS_OK) {
            initial_total_energy_ = quantities.kinetic_energy + potential_energy;
            diagnostics_.energy_available = true;
        }
    }
    energy_tracking_steps_ = 0;
}

void SimulationController::refresh_numerical_diagnostics() {
    NBodySimProParticleSystemView view = particles_.view();
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    NBodySimProDiagnosticsQuantities quantities;
    n_body_sim_pro_diagnostics_compute_global(&view, &quantities, &error);

    const double momentum_delta_x = quantities.total_momentum_x - initial_momentum_.x;
    const double momentum_delta_y = quantities.total_momentum_y - initial_momentum_.y;
    const double momentum_delta_z = quantities.total_momentum_z - initial_momentum_.z;
    diagnostics_.momentum_error =
        std::sqrt(momentum_delta_x * momentum_delta_x + momentum_delta_y * momentum_delta_y +
                  momentum_delta_z * momentum_delta_z) /
        momentum_scale_;

    const double com_delta_x = quantities.center_of_mass_x - initial_center_of_mass_.x;
    const double com_delta_y = quantities.center_of_mass_y - initial_center_of_mass_.y;
    const double com_delta_z = quantities.center_of_mass_z - initial_center_of_mass_.z;
    diagnostics_.center_of_mass_offset =
        std::sqrt(com_delta_x * com_delta_x + com_delta_y * com_delta_y +
                  com_delta_z * com_delta_z);

    diagnostics_.kinetic_energy = quantities.kinetic_energy;

    ++energy_tracking_steps_;
    if (diagnostics_.energy_available &&
        energy_tracking_steps_ >= ENERGY_TRACK_INTERVAL) {
        energy_tracking_steps_ = 0;
        double potential_energy = 0.0;
        if (n_body_sim_pro_diagnostics_compute_potential_energy(&view, &gravity_,
                                                        &potential_energy,
                                                        &error) == N_BODY_SIM_PRO_STATUS_OK) {
            const double total_energy = quantities.kinetic_energy + potential_energy;
            const double reference_energy =
                std::fabs(initial_total_energy_) > 0.0 ? std::fabs(initial_total_energy_)
                                                       : 1.0;
            diagnostics_.energy_drift =
                std::fabs(total_energy - initial_total_energy_) / reference_energy;
        }
    }
}

void SimulationController::record_trail_positions() {
    for (std::size_t body = 0; body < 2; ++body) {
        const NBodySimProVector3 position = particles_.position(body);
        rendering::Vec3 trail_point{static_cast<float>(position.x),
                                    static_cast<float>(position.y),
                                    static_cast<float>(position.z)};
        if (trails_[body].size() >= TRAIL_CAPACITY) {
            trails_[body].erase(trails_[body].begin());
        }
        trails_[body].push_back(trail_point);
    }
}

void SimulationController::clear_trails() {
    for (auto& trail : trails_) {
        trail.clear();
    }
}

}  // namespace n_body_sim_pro
