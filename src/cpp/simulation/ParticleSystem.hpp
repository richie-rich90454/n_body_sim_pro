#pragma once

#include <hpcsim/hpcsim.h>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace hpcsim {

/*
 * RAII wrapper around the C engine's HpcsimParticleSystem.
 *
 * Owns the underlying C object; copy is disabled, move transfers ownership.
 * C-level failures are surfaced as std::runtime_error with the error message
 * carried by the C error struct.
 */

class ParticleSystem final {
public:
    explicit ParticleSystem(std::size_t particle_count);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    ParticleSystem(ParticleSystem&& other) noexcept;
    ParticleSystem& operator=(ParticleSystem&& other) noexcept;

    std::size_t particle_count() const;
    std::size_t capacity() const;

    void set_particle_count(std::size_t count);
    void set_position(std::size_t index, HpcsimVector3 position);
    void set_velocity(std::size_t index, HpcsimVector3 velocity);
    void set_acceleration(std::size_t index, HpcsimVector3 acceleration);
    void set_mass(std::size_t index, double mass);

    HpcsimVector3 position(std::size_t index) const;
    HpcsimVector3 velocity(std::size_t index) const;
    HpcsimVector3 acceleration(std::size_t index) const;
    double mass(std::size_t index) const;

    /* Non-owning snapshot of the raw SoA storage for kernels and rendering. */
    HpcsimParticleSystemView view();

    void* handle() const { return handle_; }

private:
    void throw_if_failed(HpcsimStatus status, HpcsimError& error) const;

    HpcsimParticleSystem* handle_;
};

}  // namespace hpcsim
