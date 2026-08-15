#include "simulation/ParticleSystem.hpp"

namespace n_body_sim_pro {

ParticleSystem::ParticleSystem(std::size_t particle_count)
    : handle_(n_body_sim_pro_particle_system_create(particle_count)) {
    if (handle_ == nullptr) {
        throw std::runtime_error("ParticleSystem: failed to allocate particle storage");
    }
}

ParticleSystem::~ParticleSystem() {
    n_body_sim_pro_particle_system_destroy(handle_);
}

ParticleSystem::ParticleSystem(ParticleSystem&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

ParticleSystem& ParticleSystem::operator=(ParticleSystem&& other) noexcept {
    if (this != &other) {
        n_body_sim_pro_particle_system_destroy(handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

std::size_t ParticleSystem::particle_count() const {
    return n_body_sim_pro_particle_system_particle_count(handle_);
}

std::size_t ParticleSystem::capacity() const {
    return n_body_sim_pro_particle_system_capacity(handle_);
}

void ParticleSystem::set_particle_count(std::size_t count) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_set_particle_count(handle_, count, &error), error);
}

void ParticleSystem::set_position(std::size_t index, NBodySimProVector3 position) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_set_position(handle_, index, position, &error),
                    error);
}

void ParticleSystem::set_velocity(std::size_t index, NBodySimProVector3 velocity) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_set_velocity(handle_, index, velocity, &error),
                    error);
}

void ParticleSystem::set_acceleration(std::size_t index, NBodySimProVector3 acceleration) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(
        n_body_sim_pro_particle_system_set_acceleration(handle_, index, acceleration, &error), error);
}

void ParticleSystem::set_mass(std::size_t index, double mass) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_set_mass(handle_, index, mass, &error), error);
}

NBodySimProVector3 ParticleSystem::position(std::size_t index) const {
    NBodySimProVector3 position{0.0, 0.0, 0.0};
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_position(handle_, index, &position, &error), error);
    return position;
}

NBodySimProVector3 ParticleSystem::velocity(std::size_t index) const {
    NBodySimProVector3 velocity{0.0, 0.0, 0.0};
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_velocity(handle_, index, &velocity, &error), error);
    return velocity;
}

NBodySimProVector3 ParticleSystem::acceleration(std::size_t index) const {
    NBodySimProVector3 acceleration{0.0, 0.0, 0.0};
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_acceleration(handle_, index, &acceleration, &error),
                    error);
    return acceleration;
}

double ParticleSystem::mass(std::size_t index) const {
    double mass = 0.0;
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_mass(handle_, index, &mass, &error), error);
    return mass;
}

NBodySimProParticleSystemView ParticleSystem::view() {
    NBodySimProParticleSystemView view{};
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);
    throw_if_failed(n_body_sim_pro_particle_system_view(handle_, &view, &error), error);
    return view;
}

void ParticleSystem::throw_if_failed(NBodySimProStatus status, NBodySimProError& error) const {
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        throw std::runtime_error(std::string("ParticleSystem: ") + n_body_sim_pro_status_string(status) +
                                 (error.message[0] != '\0' ? std::string(": ") + error.message
                                                          : std::string()));
    }
}

}  // namespace n_body_sim_pro
