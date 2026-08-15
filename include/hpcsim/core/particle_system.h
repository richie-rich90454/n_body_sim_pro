#ifndef HPCSIM_CORE_PARTICLE_SYSTEM_H
#define HPCSIM_CORE_PARTICLE_SYSTEM_H

#include "hpcsim/core/status.h"
#include "hpcsim/core/vector.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Particle storage in Structure-of-Arrays (SoA) form.
 *
 * Layout rationale:
 *   - SIMD kernels operate on one physical quantity at a time (all x
 *     positions, then all y positions, ...); SoA keeps each stream contiguous
 *     so vector loads/stores stay dense and cache-friendly.
 *   - Physical quantities of a single particle are updated at different
 *     times during a timestep (forces accumulate into acceleration, then
 *     integration reads position+velocity); separating them avoids writing
 *     entire large particles when only one quantity changes.
 *
 * Every SoA array is 64-byte aligned. All quantities are double precision.
 * The type is opaque; access goes through the functions below.
 */

typedef struct HpcsimParticleSystem HpcsimParticleSystem;

/*
 * The default alignment for particle storage arrays. 64 bytes exceeds AVX-512
 * width (64 bytes) so any supported SIMD backend can load directly from these
 * buffers without manual alignment handling.
 */
enum { HPCSIM_PARTICLE_SYSTEM_ALIGNMENT = 64 };

/*
 * Create a particle system with storage for at most `capacity` particles.
 * The system owns its storage and must be released with
 * hpcsim_particle_system_destroy.
 *
 * Returns NULL on allocation failure or invalid capacity.
 */
HpcsimParticleSystem* hpcsim_particle_system_create(size_t capacity);

/* Release all storage owned by the particle system. NULL-safe. */
void hpcsim_particle_system_destroy(HpcsimParticleSystem* particle_system);

/* Number of particles currently stored in the system. */
size_t hpcsim_particle_system_particle_count(const HpcsimParticleSystem* particle_system);

/* Maximum number of particles the system can store without reallocation. */
size_t hpcsim_particle_system_capacity(const HpcsimParticleSystem* particle_system);

/*
 * Grow storage so that at least `capacity` particles fit, preserving existing
 * particles. Returns HPCSIM_STATUS_OK on success.
 */
HpcsimStatus hpcsim_particle_system_reserve(HpcsimParticleSystem* particle_system,
                                            size_t capacity, HpcsimError* error);

/*
 * Set the active particle count. `count` must not exceed capacity.
 * Particles beyond the count keep their previous values; the active region is
 * what every simulation kernel reads and writes.
 */
HpcsimStatus hpcsim_particle_system_set_particle_count(HpcsimParticleSystem* particle_system,
                                                       size_t count, HpcsimError* error);

/*
 * Scalar accessors. `index` must be less than the current particle count.
 * The setter variants also accept NaN-free masses and are bounds-checked.
 */
HpcsimStatus hpcsim_particle_system_set_position(HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3 position,
                                                 HpcsimError* error);

HpcsimStatus hpcsim_particle_system_set_velocity(HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3 velocity,
                                                 HpcsimError* error);

HpcsimStatus hpcsim_particle_system_set_acceleration(HpcsimParticleSystem* particle_system,
                                                     size_t index, HpcsimVector3 acceleration,
                                                     HpcsimError* error);

HpcsimStatus hpcsim_particle_system_set_mass(HpcsimParticleSystem* particle_system,
                                             size_t index, double mass, HpcsimError* error);

HpcsimStatus hpcsim_particle_system_position(const HpcsimParticleSystem* particle_system,
                                             size_t index, HpcsimVector3* position,
                                             HpcsimError* error);

HpcsimStatus hpcsim_particle_system_velocity(const HpcsimParticleSystem* particle_system,
                                             size_t index, HpcsimVector3* velocity,
                                             HpcsimError* error);

HpcsimStatus hpcsim_particle_system_acceleration(const HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3* acceleration,
                                                 HpcsimError* error);

HpcsimStatus hpcsim_particle_system_mass(const HpcsimParticleSystem* particle_system,
                                         size_t index, double* mass, HpcsimError* error);

/*
 * Raw SoA storage accessors for performance kernels.
 *
 * These expose the underlying contiguous arrays so numerical kernels and the
 * renderer can stream over them directly. Kernels MUST NOT read or write
 * beyond `particle_count`. The arrays are only valid for the lifetime of the
 * particle system and remain valid until the next call to
 * hpcsim_particle_system_reserve.
 */
double* hpcsim_particle_system_positions_x(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_positions_y(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_positions_z(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_velocities_x(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_velocities_y(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_velocities_z(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_accelerations_x(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_accelerations_y(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_accelerations_z(HpcsimParticleSystem* particle_system);
double* hpcsim_particle_system_masses(HpcsimParticleSystem* particle_system);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_CORE_PARTICLE_SYSTEM_H */
