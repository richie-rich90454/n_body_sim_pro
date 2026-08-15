#include "hpcsim/core/particle_system.h"

#include "hpcsim/memory/allocator.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT 10

struct HpcsimParticleSystem {
    size_t particle_count;
    size_t capacity;
    double* positions_x;
    double* positions_y;
    double* positions_z;
    double* velocities_x;
    double* velocities_y;
    double* velocities_z;
    double* accelerations_x;
    double* accelerations_y;
    double* accelerations_z;
    double* masses;
};

static HpcsimStatus set_component(HpcsimParticleSystem* particle_system,
                                  size_t index, double* storage,
                                  double value, HpcsimError* error) {
    if (particle_system == NULL || storage == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system or component storage is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (index >= particle_system->particle_count) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle index out of range");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    storage[index] = value;
    return HPCSIM_STATUS_OK;
}

static HpcsimStatus get_component(const HpcsimParticleSystem* particle_system,
                                  size_t index, const double* storage,
                                  double* value, HpcsimError* error) {
    if (particle_system == NULL || storage == NULL || value == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system, component storage, or output is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (index >= particle_system->particle_count) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle index out of range");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    *value = storage[index];
    return HPCSIM_STATUS_OK;
}

HpcsimParticleSystem* hpcsim_particle_system_create(size_t capacity) {
    if (capacity == 0) {
        return NULL;
    }
    if (capacity > SIZE_MAX / (HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT * sizeof(double))) {
        return NULL;
    }

    HpcsimParticleSystem* particle_system =
        (HpcsimParticleSystem*)hpcsim_allocate(
            sizeof(HpcsimParticleSystem), HPCSIM_PARTICLE_SYSTEM_ALIGNMENT,
            HPCSIM_ALLOCATION_CATEGORY_OTHER, __FILE__, __LINE__);
    if (particle_system == NULL) {
        return NULL;
    }
    *particle_system = (HpcsimParticleSystem){
        .particle_count = 0, .capacity = 0, .positions_x = NULL};

    double** buffers[HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        &particle_system->positions_x, &particle_system->positions_y,
        &particle_system->positions_z, &particle_system->velocities_x,
        &particle_system->velocities_y, &particle_system->velocities_z,
        &particle_system->accelerations_x, &particle_system->accelerations_y,
        &particle_system->accelerations_z, &particle_system->masses};

    size_t component;
    for (component = 0; component < HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        *buffers[component] = (double*)hpcsim_allocate(
            capacity * sizeof(double), HPCSIM_PARTICLE_SYSTEM_ALIGNMENT,
            HPCSIM_ALLOCATION_CATEGORY_PARTICLE_STORAGE, __FILE__, __LINE__);
        if (*buffers[component] == NULL) {
            break;
        }
    }
    if (component < HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT) {
        for (size_t allocated = 0; allocated < component; ++allocated) {
            hpcsim_deallocate(*buffers[allocated], __FILE__, __LINE__);
        }
        hpcsim_deallocate(particle_system, __FILE__, __LINE__);
        return NULL;
    }

    particle_system->capacity = capacity;
    return particle_system;
}

void hpcsim_particle_system_destroy(HpcsimParticleSystem* particle_system) {
    if (particle_system == NULL) {
        return;
    }
    double* buffers[HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        particle_system->positions_x, particle_system->positions_y,
        particle_system->positions_z, particle_system->velocities_x,
        particle_system->velocities_y, particle_system->velocities_z,
        particle_system->accelerations_x, particle_system->accelerations_y,
        particle_system->accelerations_z, particle_system->masses};
    for (size_t component = 0; component < HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        hpcsim_deallocate(buffers[component], __FILE__, __LINE__);
    }
    hpcsim_deallocate(particle_system, __FILE__, __LINE__);
}

size_t hpcsim_particle_system_particle_count(const HpcsimParticleSystem* particle_system) {
    return particle_system == NULL ? 0 : particle_system->particle_count;
}

size_t hpcsim_particle_system_capacity(const HpcsimParticleSystem* particle_system) {
    return particle_system == NULL ? 0 : particle_system->capacity;
}

HpcsimStatus hpcsim_particle_system_reserve(HpcsimParticleSystem* particle_system,
                                            size_t capacity, HpcsimError* error) {
    if (particle_system == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (capacity == 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "capacity must be non-zero");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (capacity <= particle_system->capacity) {
        return HPCSIM_STATUS_OK;
    }

    HpcsimParticleSystem* grown = hpcsim_particle_system_create(capacity);
    if (grown == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "failed to allocate grown particle storage");
        return HPCSIM_STATUS_OUT_OF_MEMORY;
    }

    const size_t count = particle_system->particle_count;
    double* source[HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        particle_system->positions_x, particle_system->positions_y,
        particle_system->positions_z, particle_system->velocities_x,
        particle_system->velocities_y, particle_system->velocities_z,
        particle_system->accelerations_x, particle_system->accelerations_y,
        particle_system->accelerations_z, particle_system->masses};
    double* target[HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        grown->positions_x, grown->positions_y, grown->positions_z,
        grown->velocities_x, grown->velocities_y, grown->velocities_z,
        grown->accelerations_x, grown->accelerations_y, grown->accelerations_z,
        grown->masses};
    for (size_t component = 0; component < HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        memcpy(target[component], source[component], count * sizeof(double));
    }
    grown->particle_count = count;

    for (size_t component = 0; component < HPCSIM_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        hpcsim_deallocate(source[component], __FILE__, __LINE__);
    }

    *particle_system = *grown;
    hpcsim_deallocate(grown, __FILE__, __LINE__);
    return HPCSIM_STATUS_OK;
}

HpcsimStatus hpcsim_particle_system_set_particle_count(HpcsimParticleSystem* particle_system,
                                                       size_t count, HpcsimError* error) {
    if (particle_system == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (count > particle_system->capacity) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle count exceeds capacity");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    particle_system->particle_count = count;
    return HPCSIM_STATUS_OK;
}

static HpcsimStatus require_system(const HpcsimParticleSystem* particle_system,
                                   HpcsimError* error) {
    if (particle_system == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    return HPCSIM_STATUS_OK;
}

HpcsimStatus hpcsim_particle_system_set_position(HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3 position,
                                                 HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = set_component(particle_system, index,
                                        particle_system->positions_x, position.x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->positions_y,
                           position.y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->positions_z,
                         position.z, error);
}

HpcsimStatus hpcsim_particle_system_set_velocity(HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3 velocity,
                                                 HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = set_component(particle_system, index,
                                        particle_system->velocities_x, velocity.x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->velocities_y,
                           velocity.y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->velocities_z,
                         velocity.z, error);
}

HpcsimStatus hpcsim_particle_system_set_acceleration(HpcsimParticleSystem* particle_system,
                                                     size_t index, HpcsimVector3 acceleration,
                                                     HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = set_component(particle_system, index,
                                        particle_system->accelerations_x, acceleration.x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->accelerations_y,
                           acceleration.y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->accelerations_z,
                         acceleration.z, error);
}

HpcsimStatus hpcsim_particle_system_set_mass(HpcsimParticleSystem* particle_system,
                                             size_t index, double mass, HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    return set_component(particle_system, index, particle_system->masses, mass, error);
}

HpcsimStatus hpcsim_particle_system_position(const HpcsimParticleSystem* particle_system,
                                             size_t index, HpcsimVector3* position,
                                             HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (position == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output position is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = get_component(particle_system, index,
                                        particle_system->positions_x, &position->x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->positions_y,
                           &position->y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->positions_z,
                         &position->z, error);
}

HpcsimStatus hpcsim_particle_system_velocity(const HpcsimParticleSystem* particle_system,
                                             size_t index, HpcsimVector3* velocity,
                                             HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (velocity == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output velocity is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = get_component(particle_system, index,
                                        particle_system->velocities_x, &velocity->x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->velocities_y,
                           &velocity->y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->velocities_z,
                         &velocity->z, error);
}

HpcsimStatus hpcsim_particle_system_acceleration(const HpcsimParticleSystem* particle_system,
                                                 size_t index, HpcsimVector3* acceleration,
                                                 HpcsimError* error) {
    if (require_system(particle_system, error) != HPCSIM_STATUS_OK) {
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (acceleration == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output acceleration is null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    HpcsimStatus status = get_component(particle_system, index,
                                        particle_system->accelerations_x, &acceleration->x, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->accelerations_y,
                           &acceleration->y, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->accelerations_z,
                         &acceleration->z, error);
}

HpcsimStatus hpcsim_particle_system_mass(const HpcsimParticleSystem* particle_system,
                                         size_t index, double* mass, HpcsimError* error) {
    return get_component(particle_system, index, particle_system->masses, mass, error);
}

#define HPCSIM_PARTICLE_SYSTEM_ACCESSOR(qualifier, name)                           \
    double* hpcsim_particle_system_##name(qualifier HpcsimParticleSystem* system) \
    {                                                                              \
        return system == NULL ? NULL : system->name;                               \
    }

HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, positions_x)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, positions_y)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, positions_z)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, velocities_x)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, velocities_y)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, velocities_z)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, accelerations_x)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, accelerations_y)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, accelerations_z)
HPCSIM_PARTICLE_SYSTEM_ACCESSOR(, masses)

#undef HPCSIM_PARTICLE_SYSTEM_ACCESSOR
