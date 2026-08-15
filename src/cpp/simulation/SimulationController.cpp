#include "simulation/SimulationController.hpp"

#include "logging/Logger.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace hpcsim {

SimulationController::SimulationController() : particles_(2) {
    recompute_gravity_parameters();
    for (auto& trail : trails_) {
        trail.reserve(TRAIL_CAPACITY);
    }
    const HpcsimCpuFeatures cpu_features = hpcsim_cpu_detect_features();
    simd_backend_ = hpcsim_simd_best_available_backend(&cpu_features);
    HPCSIM_LOG(logging::Level::Info, logging::Category::Simd,
               "Selected SIMD backend: %s", hpcsim_simd_backend_string(simd_backend_));
    HPCSIM_LOG(logging::Level::Info, logging::Category::Threading, "OpenMP available: %s",
               hpcsim_threading_openmp_available() ? "yes" : "no");
    apply_preset(HPCSIM_PRESET_TWO_BODY, 2, 1);
}

SimulationController::~SimulationController() = default;

void SimulationController::apply_preset(HpcsimSimulationPreset preset,
                                        std::size_t particle_count,
                                        std::uint64_t random_seed) {
    if (particle_count == 0) {
        throw std::runtime_error("SimulationController: preset particle count is zero");
    }
    ParticleSystem replacement(particle_count);

    HpcsimPresetParameters parameters;
    parameters.particle_count = particle_count;
    parameters.random_seed = random_seed;

    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimStatus status = hpcsim_preset_generate(
        replacement.handle(), preset, &parameters, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: preset generation "
                                             "failed: ") +
                                 hpcsim_status_string(status));
    }

    particles_ = std::move(replacement);
    preset_ = preset;
    random_seed_ = random_seed;
    simulation_time = 0.0;
    clear_trails();
    compute_initial_accelerations();
    reset_diagnostics_reference();
    HPCSIM_LOG(logging::Level::Info, logging::Category::Simulation,
               "Initialized preset %s with %zu particles (seed %llu)",
               hpcsim_preset_string(preset), particle_count,
               (unsigned long long)random_seed);
}

HpcsimBarnesHutTree* SimulationController::barnes_hut_tree() {
    if (!tree_) {
        HpcsimError error;
        hpcsim_error_clear(&error);
        tree_.reset(hpcsim_barnes_hut_tree_create(&error));
        if (!tree_) {
            throw std::runtime_error("SimulationController: failed to create Barnes-Hut "
                                     "tree context");
        }
    }
    hpcsim_barnes_hut_tree_set_theta(tree_.get(), barnes_hut_theta);
    return tree_.get();
}

HpcsimForceFunction SimulationController::select_force_function(void*& force_context) const {
    if (barnes_hut_enabled) {
        force_context = const_cast<HpcsimBarnesHutTree*>(tree_.get());
        return hpcsim_barnes_hut_compute_acceleration;
    }
    force_context = nullptr;
    const bool avx2_available = simd_backend_ == HPCSIM_SIMD_BACKEND_AVX2;
    const bool openmp_available = hpcsim_threading_openmp_available() != 0;
    if (use_parallel_forces && openmp_available) {
        return avx2_available ? hpcsim_gravity_compute_acceleration_openmp_avx2
                              : hpcsim_gravity_compute_acceleration_openmp;
    }
    if (avx2_available) {
        return hpcsim_gravity_compute_acceleration_avx2;
    }
    return hpcsim_gravity_compute_acceleration_reference;
}

void SimulationController::compute_initial_accelerations() {
    if (barnes_hut_enabled) {
        hpcsim_barnes_hut_tree_set_theta(barnes_hut_tree(), barnes_hut_theta);
    }
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    void* force_context = nullptr;
    HpcsimForceFunction force_function = select_force_function(force_context);
    HpcsimStatus status = force_function(&view, &gravity_, force_context, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error("SimulationController: failed to compute initial forces");
    }
}

void SimulationController::recompute_gravity_parameters() {
    hpcsim_gravity_init(&gravity_, gravitational_constant_, softening_length_);
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
        hpcsim_barnes_hut_tree_set_theta(barnes_hut_tree(), barnes_hut_theta);
    }
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    void* force_context = nullptr;
    HpcsimForceFunction force_function = select_force_function(force_context);

    const auto step_start = std::chrono::steady_clock::now();
    HpcsimStatus status = hpcsim_integrator_advance(&view, &gravity_, integrator, timestep,
                                                    force_function, force_context, &error);
    const auto step_end = std::chrono::steady_clock::now();
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error("SimulationController: integrator step failed");
    }

    last_step_ms_ = std::chrono::duration<double, std::milli>(step_end - step_start).count();
    if (barnes_hut_enabled) {
        HpcsimBarnesHutStats stats;
        if (hpcsim_barnes_hut_tree_stats(tree_.get(), &stats)) {
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
    if (preset_ == HPCSIM_PRESET_TWO_BODY) {
        record_trail_positions();
    }
}

void SimulationController::refresh_energy_diagnostics() {
    if (!diagnostics_.energy_available) {
        return;
    }
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimDiagnosticsQuantities quantities;
    hpcsim_diagnostics_compute_global(&view, &quantities, &error);
    double potential_energy = 0.0;
    if (hpcsim_diagnostics_compute_potential_energy(&view, &gravity_, &potential_energy,
                                                    &error) == HPCSIM_STATUS_OK) {
        const double total_energy = quantities.kinetic_energy + potential_energy;
        const double reference_energy =
            std::fabs(initial_total_energy_) > 0.0 ? std::fabs(initial_total_energy_) : 1.0;
        diagnostics_.energy_drift =
            std::fabs(total_energy - initial_total_energy_) / reference_energy;
    }
}

void SimulationController::save_checkpoint(const char* path) {
    HpcsimCheckpointHeader header{};
    header.magic = HPCSIM_CHECKPOINT_MAGIC;
    header.version = HPCSIM_CHECKPOINT_VERSION;
    header.particle_count = particles_.particle_count();
    header.simulation_time = simulation_time;
    header.timestep = timestep;
    header.integrator = static_cast<int32_t>(integrator);
    header.theta = barnes_hut_theta;
    header.barnes_hut_enabled = barnes_hut_enabled ? 1 : 0;
    header.random_seed = random_seed_;
    header.preset = static_cast<int32_t>(preset_);

    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    const HpcsimStatus status =
        hpcsim_checkpoint_write(path, &view, &header, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint write "
                                             "failed: ") +
                                 hpcsim_status_string(status));
    }
    HPCSIM_LOG(logging::Level::Info, logging::Category::Simulation,
               "Checkpoint saved to %s (%zu particles, t=%.4f)", path,
               header.particle_count, simulation_time);
}

void SimulationController::load_checkpoint(const char* path) {
    HpcsimCheckpointHeader header{};
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimStatus status = hpcsim_checkpoint_peek(path, &header, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint peek "
                                             "failed: ") +
                                 hpcsim_status_string(status));
    }
    ParticleSystem replacement(header.particle_count);
    hpcsim_error_clear(&error);
    status = hpcsim_checkpoint_read(path, &header, replacement.handle(), &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error(std::string("SimulationController: checkpoint read "
                                             "failed: ") +
                                 hpcsim_status_string(status));
    }

    particles_ = std::move(replacement);
    simulation_time = header.simulation_time;
    timestep = header.timestep;
    integrator = static_cast<HpcsimIntegratorType>(header.integrator);
    barnes_hut_theta = header.theta;
    barnes_hut_enabled = header.barnes_hut_enabled != 0;
    random_seed_ = header.random_seed;
    preset_ = static_cast<HpcsimSimulationPreset>(header.preset);

    clear_trails();
    reset_diagnostics_reference();
    HPCSIM_LOG(logging::Level::Info, logging::Category::Simulation,
               "Checkpoint loaded from %s (%zu particles, t=%.4f)", path,
               header.particle_count, simulation_time);
}

void SimulationController::reset_diagnostics_reference() {
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimDiagnosticsQuantities quantities;
    hpcsim_diagnostics_compute_global(&view, &quantities, &error);

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
        if (hpcsim_diagnostics_compute_potential_energy(&view, &gravity_,
                                                        &potential_energy,
                                                        &error) == HPCSIM_STATUS_OK) {
            initial_total_energy_ = quantities.kinetic_energy + potential_energy;
            diagnostics_.energy_available = true;
        }
    }
    energy_tracking_steps_ = 0;
}

void SimulationController::refresh_numerical_diagnostics() {
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimDiagnosticsQuantities quantities;
    hpcsim_diagnostics_compute_global(&view, &quantities, &error);

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
        if (hpcsim_diagnostics_compute_potential_energy(&view, &gravity_,
                                                        &potential_energy,
                                                        &error) == HPCSIM_STATUS_OK) {
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
        const HpcsimVector3 position = particles_.position(body);
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

}  // namespace hpcsim
