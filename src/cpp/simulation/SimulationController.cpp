#include "simulation/SimulationController.hpp"

#include <stdexcept>

namespace hpcsim {

SimulationController::SimulationController() : particles_(2) {
    recompute_gravity_parameters();
    for (auto& trail : trails_) {
        trail.reserve(TRAIL_CAPACITY);
    }
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

void SimulationController::compute_initial_accelerations() {
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimStatus status =
        hpcsim_gravity_compute_acceleration_reference(&view, &gravity_, &error);
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
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimForceFunction force_function =
        use_parallel_forces && hpcsim_threading_openmp_available()
            ? hpcsim_gravity_compute_acceleration_openmp
            : hpcsim_gravity_compute_acceleration_reference;
    HpcsimStatus status = hpcsim_integrator_advance(&view, &gravity_, integrator, timestep,
                                                    force_function, &error);
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
