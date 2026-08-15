#pragma once

#include "rendering/matrix.hpp"
#include "simulation/ParticleSystem.hpp"

#include <hpcsim/hpcsim.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace hpcsim {

/*
 * Drives the C physics engine and owns the state the renderer visualizes.
 *
 * The controller owns the particle system, the gravity parameters, the
 * integrator, and the time state. It does not know about SDL, OpenGL, or
 * ImGui: the user interface and the renderer read state from it. The system
 * is re-initialized from a preset on demand.
 */

class SimulationController final {
public:
    SimulationController();
    ~SimulationController();

    SimulationController(const SimulationController&) = delete;
    SimulationController& operator=(const SimulationController&) = delete;

    /* (Re)initialize the simulation from a preset. */
    void apply_preset(HpcsimSimulationPreset preset, std::size_t particle_count,
                      std::uint64_t random_seed);

    /* Advance the system by one timestep using the configured integrator. */
    void step();

    ParticleSystem& particle_system() { return particles_; }
    const ParticleSystem& particle_system() const { return particles_; }

    HpcsimSimulationPreset preset() const { return preset_; }
    std::uint64_t random_seed() const { return random_seed_; }

    bool running = true;
    double simulation_time = 0.0;
    double timestep = 0.005;

    /* Use the OpenMP-parallel force kernel when available. */
    bool use_parallel_forces = true;

    /* Use the Barnes-Hut octree approximation instead of all-pairs. */
    bool barnes_hut_enabled = false;
    double barnes_hut_theta = 0.7;

    /* The SIMD backend detected at construction (AVX2 or scalar). */
    HpcsimSimdBackend simd_backend() const { return simd_backend_; }

    HpcsimIntegratorType integrator = HPCSIM_INTEGRATOR_LEAPFROG;

    double gravitational_constant() const { return gravitational_constant_; }
    double softening_length() const { return softening_length_; }
    void set_gravitational_constant(double value);
    void set_softening_length(double value);

    const HpcsimGravity& gravity() const { return gravity_; }

    /* Statistics from the most recent Barnes-Hut force evaluation. */
    bool barnes_hut_stats(HpcsimBarnesHutStats* stats) const {
        return hpcsim_barnes_hut_tree_stats(tree_.get(), stats) == 0;
    }

    /* Per-body trajectory trail for the two-body preset. */
    const std::array<std::vector<rendering::Vec3>, 2>& trails() const { return trails_; }
    void clear_trails();

private:
    void recompute_gravity_parameters();
    void compute_initial_accelerations();
    void record_trail_positions();
    HpcsimForceFunction select_force_function(void*& force_context) const;
    HpcsimBarnesHutTree* barnes_hut_tree();

    ParticleSystem particles_;
    HpcsimGravity gravity_;
    std::unique_ptr<HpcsimBarnesHutTree, void (*)(HpcsimBarnesHutTree*)> tree_{
        nullptr, hpcsim_barnes_hut_tree_destroy};
    double gravitational_constant_ = 1.0;
    double softening_length_ = 0.0;
    HpcsimSimulationPreset preset_ = HPCSIM_PRESET_TWO_BODY;
    HpcsimSimdBackend simd_backend_ = HPCSIM_SIMD_BACKEND_SCALAR;
    std::uint64_t random_seed_ = 0;
    std::array<std::vector<rendering::Vec3>, 2> trails_;
    static constexpr std::size_t TRAIL_CAPACITY = 4096;
};

}  // namespace hpcsim
