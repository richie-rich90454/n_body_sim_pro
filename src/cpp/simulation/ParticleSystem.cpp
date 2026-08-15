#include "simulation/ParticleSystem.hpp"

namespace hpcsim {

ParticleSystem::ParticleSystem(std::size_t particle_count)
    : handle_(hpcsim_particle_system_create(particle_count)) {
    if (handle_ == nullptr) {
        throw std::runtime_error("ParticleSystem: failed to allocate particle storage");
    }
}

ParticleSystem::~ParticleSystem() {
    hpcsim_particle_system_destroy(handle_);
}

ParticleSystem::ParticleSystem(ParticleSystem&& other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
}

ParticleSystem& ParticleSystem::operator=(ParticleSystem&& other) noexcept {
    if (this != &other) {
        hpcsim_particle_system_destroy(handle_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }
    return *this;
}

std::size_t ParticleSystem::particle_count() const {
    return hpcsim_particle_system_particle_count(handle_);
}

std::size_t ParticleSystem::capacity() const {
    return hpcsim_particle_system_capacity(handle_);
}

void ParticleSystem::set_particle_count(std::size_t count) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_set_particle_count(handle_, count, &error), error);
}

void ParticleSystem::set_position(std::size_t index, HpcsimVector3 position) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_set_position(handle_, index, position, &error),
                    error);
}

void ParticleSystem::set_velocity(std::size_t index, HpcsimVector3 velocity) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_set_velocity(handle_, index, velocity, &error),
                    error);
}

void ParticleSystem::set_acceleration(std::size_t index, HpcsimVector3 acceleration) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(
        hpcsim_particle_system_set_acceleration(handle_, index, acceleration, &error), error);
}

void ParticleSystem::set_mass(std::size_t index, double mass) {
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_set_mass(handle_, index, mass, &error), error);
}

HpcsimVector3 ParticleSystem::position(std::size_t index) const {
    HpcsimVector3 position{0.0, 0.0, 0.0};
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_position(handle_, index, &position, &error), error);
    return position;
}

HpcsimVector3 ParticleSystem::velocity(std::size_t index) const {
    HpcsimVector3 velocity{0.0, 0.0, 0.0};
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_velocity(handle_, index, &velocity, &error), error);
    return velocity;
}

HpcsimVector3 ParticleSystem::acceleration(std::size_t index) const {
    HpcsimVector3 acceleration{0.0, 0.0, 0.0};
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_acceleration(handle_, index, &acceleration, &error),
                    error);
    return acceleration;
}

double ParticleSystem::mass(std::size_t index) const {
    double mass = 0.0;
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_mass(handle_, index, &mass, &error), error);
    return mass;
}

HpcsimParticleSystemView ParticleSystem::view() {
    HpcsimParticleSystemView view{};
    HpcsimError error;
    hpcsim_error_clear(&error);
    throw_if_failed(hpcsim_particle_system_view(handle_, &view, &error), error);
    return view;
}

void ParticleSystem::throw_if_failed(HpcsimStatus status, HpcsimError& error) const {
    if (status != HPCSIM_STATUS_OK) {
        throw std::runtime_error(std::string("ParticleSystem: ") + hpcsim_status_string(status) +
                                 (error.message[0] != '\0' ? std::string(": ") + error.message
                                                          : std::string()));
    }
}

}  // namespace hpcsim
