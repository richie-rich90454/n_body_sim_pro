#include "simulation/SimulationController.hpp"

#include <cmath>

namespace hpcsim {

namespace {
constexpr double PI = 3.14159265358979323846;
}

SimulationController::SimulationController() : particles_(2) {
    recompute_gravity_parameters();
    for (auto& trail : trails_) {
        trail.reserve(TRAIL_CAPACITY);
    }
    initialize_two_body();
}

SimulationController::~SimulationController() = default;

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

void SimulationController::initialize_two_body() {
    particles_.set_particle_count(2);

    const double half_separation = 0.5;
    const double per_body_speed = std::sqrt(2.0) / 2.0;

    particles_.set_position(0, {-half_separation, 0.0, 0.0});
    particles_.set_position(1, {half_separation, 0.0, 0.0});
    particles_.set_velocity(0, {0.0, per_body_speed, 0.0});
    particles_.set_velocity(1, {0.0, -per_body_speed, 0.0});
    particles_.set_mass(0, 1.0);
    particles_.set_mass(1, 1.0);

    simulation_time = 0.0;
    clear_trails();

    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimStatus status =
        hpcsim_gravity_compute_acceleration_reference(&view, &gravity_, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error("SimulationController: failed to compute initial forces");
    }
}

void SimulationController::step() {
    HpcsimParticleSystemView view = particles_.view();
    HpcsimError error;
    hpcsim_error_clear(&error);
    HpcsimStatus status = hpcsim_integrator_advance(
        &view, &gravity_, integrator, timestep,
        hpcsim_gravity_compute_acceleration_reference, &error);
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error("SimulationController: integrator step failed");
    }
    simulation_time += timestep;
    record_trail_positions();
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
