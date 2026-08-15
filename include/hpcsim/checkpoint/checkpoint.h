#ifndef HPCSIM_CHECKPOINT_CHECKPOINT_H
#define HPCSIM_CHECKPOINT_CHECKPOINT_H

#include "hpcsim/core/particle_system.h"
#include "hpcsim/core/status.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simulation checkpointing.
 *
 * A checkpoint captures the full simulation state needed to resume exactly:
 * particle positions, velocities, accelerations, masses, the simulation
 * time and timestep, the integrator, the Barnes-Hut opening angle, the
 * random seed, and the preset. The file format is versioned so future
 * engine versions can read older checkpoints where possible.
 *
 * Format (little-endian):
 *   magic "HPCS" (4 bytes)
 *   version (uint32)
 *   header fields (uint64/uint32/...)
 *   positions_x/y/z, velocities_x/y/z, accelerations_x/y/z, masses
 *     (particle_count doubles each)
 */

enum { HPCSIM_CHECKPOINT_MAGIC = 0x53504348, HPCSIM_CHECKPOINT_VERSION = 1 };

typedef struct HpcsimCheckpointHeader {
    uint32_t magic;
    uint32_t version;
    size_t particle_count;
    double simulation_time;
    double timestep;
    int32_t integrator;
    double theta;
    int32_t barnes_hut_enabled;
    uint64_t random_seed;
    int32_t preset;
} HpcsimCheckpointHeader;

/*
 * Write the particle state and header to `path`. Overwrites an existing
 * file. Returns HPCSIM_STATUS_OK on success.
 */
HpcsimStatus hpcsim_checkpoint_write(const char* path,
                                     const HpcsimParticleSystemView* view,
                                     const HpcsimCheckpointHeader* header,
                                     HpcsimError* error);

/*
 * Read only the checkpoint header (magic, version, metadata) without the
 * particle arrays, so a caller can size storage before a full read.
 */
HpcsimStatus hpcsim_checkpoint_peek(const char* path, HpcsimCheckpointHeader* header,
                                    HpcsimError* error);

/*
 * Read a checkpoint. `particle_system` must be created with at least the
 * checkpoint's particle count capacity; its storage is filled from the file.
 * On success *header carries the stored metadata. Returns HPCSIM_STATUS_OK.
 */
HpcsimStatus hpcsim_checkpoint_read(const char* path, HpcsimCheckpointHeader* header,
                                    HpcsimParticleSystem* particle_system,
                                    HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_CHECKPOINT_CHECKPOINT_H */
