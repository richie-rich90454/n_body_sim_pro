#ifndef N_BODY_SIM_PRO_GENERATION_PRESETS_H
#define N_BODY_SIM_PRO_GENERATION_PRESETS_H

#include "n_body_sim_pro/core/particle_system.h"
#include "n_body_sim_pro/core/status.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initial-condition presets.
 *
 * These are simplified educational models, not scientifically exact
 * astronomical reproductions. Each preset documents its assumptions in
 * docs/physics/presets.md. All presets are deterministic: the same preset
 * and seed always produce the same initial conditions.
 *
 * Units are internally consistent (solar masses, parsecs, G chosen so
 * circular speeds come out in km/s-ish scale); the exact scale is a display
 * choice, not a claim about real astronomy.
 */

typedef enum NBodySimProSimulationPreset {
    N_BODY_SIM_PRO_PRESET_TWO_BODY,
    N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD,
    N_BODY_SIM_PRO_PRESET_SOLAR_SYSTEM,
    N_BODY_SIM_PRO_PRESET_OPEN_CLUSTER,
    N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER,
    N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY,
    N_BODY_SIM_PRO_PRESET_ELLIPTICAL_GALAXY,
    N_BODY_SIM_PRO_PRESET_GALAXY_COLLISION,
    N_BODY_SIM_PRO_PRESET_TRIPLE_GALAXY,
    N_BODY_SIM_PRO_PRESET_COUNT
} NBodySimProSimulationPreset;

/* Human-readable preset name. Never returns NULL. */
const char* n_body_sim_pro_preset_string(NBodySimProSimulationPreset preset);

typedef struct NBodySimProPresetParameters {
    size_t particle_count;
    uint64_t random_seed;
} NBodySimProPresetParameters;

/*
 * Generate initial conditions for `preset` into `particle_system`.
 *
 * `particle_system` must be created with at least `parameters.particle_count`
 * capacity. On success its particle count is set to `particle_count`.
 * All generated positions, velocities, and masses are written through the
 * system's setters.
 */
NBodySimProStatus n_body_sim_pro_preset_generate(NBodySimProParticleSystem* particle_system,
                                    NBodySimProSimulationPreset preset,
                                    const NBodySimProPresetParameters* parameters,
                                    NBodySimProError* error);

/*
 * Parallel first-touch-aware generation.
 *
 * Particles are generated with OpenMP; each thread writes its own slice of
 * the SoA arrays, which establishes NUMA page placement on the node of the
 * thread that will process it. Every particle's randomness is derived
 * deterministically from (random_seed, particle index), so the result is
 * reproducible across thread counts and matches the sequential generator's
 * statistical distributions. The exact sequence differs from
 * n_body_sim_pro_preset_generate (which uses a single sequential PRNG stream); the
 * sequential generator remains the correctness reference.
 */
NBodySimProStatus n_body_sim_pro_preset_generate_parallel(NBodySimProParticleSystem* particle_system,
                                             NBodySimProSimulationPreset preset,
                                             const NBodySimProPresetParameters* parameters,
                                             NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_GENERATION_PRESETS_H */
