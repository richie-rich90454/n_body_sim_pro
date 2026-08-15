#include "simulation/SimulationController.hpp"

#include <stdexcept>

namespace hpcsim {

SimulationController::SimulationController() : particles_(2) {
    recompute_gravity_parameters();
    for (auto& trail : trails_) {
        trail.reserve(TRAIL_CAPACITY);
    }
    const HpcsimCpuFeatures cpu_features = hpcsim_cpu_detect_features();
    simd_backend_ = hpcsim_simd_best_available_backend(&cpu_features);
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
    HpcsimStatus status = hpcsim_integrator_advance(&view, &gravity_, integrator, timestep,
                                                    force_function, force_context, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error("SimulationController: integrator step failed");
    }
    simulation_time += timestep;
    if (preset_ == HPCSIM_PRESET_TWO_BODY) {
        record_trail_positions();
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
