#ifndef HPCSIM_GENERATION_PRESETS_H
#define HPCSIM_GENERATION_PRESETS_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"

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

typedef enum HpcsimSimulationPreset {
    HPCSIM_PRESET_TWO_BODY,
    HPCSIM_PRESET_RANDOM_CLOUD,
    HPCSIM_PRESET_SOLAR_SYSTEM,
    HPCSIM_PRESET_OPEN_CLUSTER,
    HPCSIM_PRESET_GLOBULAR_CLUSTER,
    HPCSIM_PRESET_SPIRAL_GALAXY,
    HPCSIM_PRESET_ELLIPTICAL_GALAXY,
    HPCSIM_PRESET_GALAXY_COLLISION,
    HPCSIM_PRESET_TRIPLE_GALAXY,
    HPCSIM_PRESET_COUNT
} HpcsimSimulationPreset;

/* Human-readable preset name. Never returns NULL. */
const char* hpcsim_preset_string(HpcsimSimulationPreset preset);

typedef struct HpcsimPresetParameters {
    size_t particle_count;
    uint64_t random_seed;
} HpcsimPresetParameters;

/*
 * Generate initial conditions for `preset` into `particle_system`.
 *
 * `particle_system` must be created with at least `parameters.particle_count`
 * capacity. On success its particle count is set to `particle_count`.
 * All generated positions, velocities, and masses are written through the
 * system's setters.
 */
HpcsimStatus hpcsim_preset_generate(HpcsimParticleSystem* particle_system,
                                    HpcsimSimulationPreset preset,
                                    const HpcsimPresetParameters* parameters,
                                    HpcsimError* error);

/*
 * Parallel first-touch-aware generation.
 *
 * Particles are generated with OpenMP; each thread writes its own slice of
 * the SoA arrays, which establishes NUMA page placement on the node of the
 * thread that will process it. Every particle's randomness is derived
 * deterministically from (random_seed, particle index), so the result is
 * reproducible across thread counts and matches the sequential generator's
 * statistical distributions. The exact sequence differs from
 * hpcsim_preset_generate (which uses a single sequential PRNG stream); the
 * sequential generator remains the correctness reference.
 */
HpcsimStatus hpcsim_preset_generate_parallel(HpcsimParticleSystem* particle_system,
                                             HpcsimSimulationPreset preset,
                                             const HpcsimPresetParameters* parameters,
                                             HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_GENERATION_PRESETS_H */
