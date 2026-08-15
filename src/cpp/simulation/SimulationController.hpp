#pragma once

#include "rendering/matrix.hpp"
#include "simulation/ParticleSystem.hpp"

#include <hpcsim/hpcsim.h>

#include <array>
#include <cstddef>
#include <vector>

namespace hpcsim {

/*
 * Drives the C physics engine and owns the state the renderer visualizes.
 *
 * The controller owns the particle system, the gravity parameters, the
 * integrator, and the time state. It does not know about SDL, OpenGL, or
 * ImGui: the user interface and the renderer read state from it.
 */

class SimulationController final {
public:
    SimulationController();
    ~SimulationController();

    SimulationController(const SimulationController&) = delete;
    SimulationController& operator=(const SimulationController&) = delete;

    /* Two equal masses on a circular orbit about their barycenter. */
    void initialize_two_body();

    /* Advance the system by one timestep using the configured integrator. */
    void step();

    ParticleSystem& particle_system() { return particles_; }
    const ParticleSystem& particle_system() const { return particles_; }

    bool running = true;
    double simulation_time = 0.0;
    double timestep = 0.005;

    HpcsimIntegratorType integrator = HPCSIM_INTEGRATOR_LEAPFROG;

    double gravitational_constant() const { return gravitational_constant_; }
    double softening_length() const { return softening_length_; }
    void set_gravitational_constant(double value);
    void set_softening_length(double value);

    const HpcsimGravity& gravity() const { return gravity_; }

    /* Per-body trajectory trail for visualization. */
    const std::array<std::vector<rendering::Vec3>, 2>& trails() const { return trails_; }
    void clear_trails();

private:
    void record_trail_positions();
    void recompute_gravity_parameters();

    ParticleSystem particles_;
    HpcsimGravity gravity_;
    double gravitational_constant_ = 1.0;
    double softening_length_ = 0.0;
    std::array<std::vector<rendering::Vec3>, 2> trails_;
    static constexpr std::size_t TRAIL_CAPACITY = 4096;
};

}  // namespace hpcsim
