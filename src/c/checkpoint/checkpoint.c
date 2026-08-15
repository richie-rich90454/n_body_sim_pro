#include "hpcsim/checkpoint/checkpoint.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { CHECKPOINT_ARRAY_COUNT = 10 };

HpcsimStatus hpcsim_checkpoint_write(const char* path,
                                     const HpcsimParticleSystemView* view,
                                     const HpcsimCheckpointHeader* header,
                                     HpcsimError* error) {
    if (path == NULL || view == NULL || header == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "path, view, and header must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (view->particle_count != header->particle_count) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "checkpoint particle count does not match the view");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to open checkpoint file for writing");
        return HPCSIM_STATUS_IO_ERROR;
    }

    const uint32_t magic = HPCSIM_CHECKPOINT_MAGIC;
    const uint32_t version = HPCSIM_CHECKPOINT_VERSION;
    if (fwrite(&magic, sizeof(magic), 1, file) != 1 ||
        fwrite(&version, sizeof(version), 1, file) != 1) {
        fclose(file);
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to write checkpoint header");
        return HPCSIM_STATUS_IO_ERROR;
    }

    const HpcsimCheckpointHeader stored = *header;
    if (fwrite(&stored, sizeof(stored), 1, file) != 1) {
        fclose(file);
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to write checkpoint metadata");
        return HPCSIM_STATUS_IO_ERROR;
    }

    const double* const arrays[CHECKPOINT_ARRAY_COUNT] = {
        view->positions_x,  view->positions_y,      view->positions_z,
        view->velocities_x, view->velocities_y,     view->velocities_z,
        view->accelerations_x, view->accelerations_y, view->accelerations_z,
        view->masses};
    const size_t bytes = view->particle_count * sizeof(double);
    for (int i = 0; i < CHECKPOINT_ARRAY_COUNT; ++i) {
        if (arrays[i] == NULL) {
            fclose(file);
            hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                             "checkpoint array is null");
            return HPCSIM_STATUS_INVALID_ARGUMENT;
        }
        if (fwrite(arrays[i], bytes, 1, file) != 1) {
            fclose(file);
            hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                             "failed to write checkpoint array");
            return HPCSIM_STATUS_IO_ERROR;
        }
    }

    if (fclose(file) != 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to close checkpoint file");
        return HPCSIM_STATUS_IO_ERROR;
    }
    return HPCSIM_STATUS_OK;
}

static int read_magic_and_version(FILE* file, HpcsimError* error) {
    uint32_t magic = 0;
    uint32_t version = 0;
    if (fread(&magic, sizeof(magic), 1, file) != 1 ||
        fread(&version, sizeof(version), 1, file) != 1) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to read checkpoint magic");
        return 0;
    }
    if (magic != HPCSIM_CHECKPOINT_MAGIC) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_STATE, __FILE__, __LINE__,
                         "not an HPCSim checkpoint file");
        return 0;
    }
    if (version != HPCSIM_CHECKPOINT_VERSION) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_STATE, __FILE__, __LINE__,
                         "unsupported checkpoint version");
        return 0;
    }
    return 1;
}

HpcsimStatus hpcsim_checkpoint_peek(const char* path, HpcsimCheckpointHeader* header,
                                    HpcsimError* error) {
    if (path == NULL || header == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "path and header must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to open checkpoint file for reading");
        return HPCSIM_STATUS_IO_ERROR;
    }
    if (!read_magic_and_version(file, error)) {
        fclose(file);
        return hpcsim_error_failed(error) ? error->status : HPCSIM_STATUS_IO_ERROR;
    }
    if (fread(header, sizeof(*header), 1, file) != 1) {
        fclose(file);
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to read checkpoint metadata");
        return HPCSIM_STATUS_IO_ERROR;
    }
    if (fclose(file) != 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to close checkpoint file");
        return HPCSIM_STATUS_IO_ERROR;
    }
    return HPCSIM_STATUS_OK;
}

HpcsimStatus hpcsim_checkpoint_read(const char* path, HpcsimCheckpointHeader* header,
                                    HpcsimParticleSystem* particle_system,
                                    HpcsimError* error) {
    if (path == NULL || header == NULL || particle_system == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "path, header, and particle system must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to open checkpoint file for reading");
        return HPCSIM_STATUS_IO_ERROR;
    }

    if (!read_magic_and_version(file, error)) {
        fclose(file);
        return hpcsim_error_failed(error) ? error->status : HPCSIM_STATUS_IO_ERROR;
    }

    HpcsimCheckpointHeader stored;
    if (fread(&stored, sizeof(stored), 1, file) != 1) {
        fclose(file);
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to read checkpoint metadata");
        return HPCSIM_STATUS_IO_ERROR;
    }
    if (hpcsim_particle_system_capacity(particle_system) < stored.particle_count) {
        fclose(file);
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system capacity is smaller than checkpoint count");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    const size_t bytes = stored.particle_count * sizeof(double);
    double* const arrays[CHECKPOINT_ARRAY_COUNT] = {
        hpcsim_particle_system_positions_x(particle_system),
        hpcsim_particle_system_positions_y(particle_system),
        hpcsim_particle_system_positions_z(particle_system),
        hpcsim_particle_system_velocities_x(particle_system),
        hpcsim_particle_system_velocities_y(particle_system),
        hpcsim_particle_system_velocities_z(particle_system),
        hpcsim_particle_system_accelerations_x(particle_system),
        hpcsim_particle_system_accelerations_y(particle_system),
        hpcsim_particle_system_accelerations_z(particle_system),
        hpcsim_particle_system_masses(particle_system)};
    for (int i = 0; i < CHECKPOINT_ARRAY_COUNT; ++i) {
        if (arrays[i] == NULL) {
            fclose(file);
            hpcsim_error_set(error, HPCSIM_STATUS_INVALID_STATE, __FILE__, __LINE__,
                             "particle system array is null");
            return HPCSIM_STATUS_INVALID_STATE;
        }
        if (bytes > 0 && fread(arrays[i], bytes, 1, file) != 1) {
            fclose(file);
            hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                             "failed to read checkpoint array");
            return HPCSIM_STATUS_IO_ERROR;
        }
    }

    if (fclose(file) != 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_IO_ERROR, __FILE__, __LINE__,
                         "failed to close checkpoint file");
        return HPCSIM_STATUS_IO_ERROR;
    }

    HpcsimStatus status = hpcsim_particle_system_set_particle_count(
        particle_system, stored.particle_count, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    *header = stored;
    return HPCSIM_STATUS_OK;
}
