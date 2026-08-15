#include "n_body_sim_pro/core/particle_system.h"

#include "n_body_sim_pro/memory/allocator.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT 10

struct NBodySimProParticleSystem {
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

static NBodySimProStatus set_component(NBodySimProParticleSystem* particle_system,
                                  size_t index, double* storage,
                                  double value, NBodySimProError* error) {
    if (particle_system == NULL || storage == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system or component storage is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (index >= particle_system->particle_count) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle index out of range");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    storage[index] = value;
    return N_BODY_SIM_PRO_STATUS_OK;
}

static NBodySimProStatus get_component(const NBodySimProParticleSystem* particle_system,
                                  size_t index, const double* storage,
                                  double* value, NBodySimProError* error) {
    if (particle_system == NULL || storage == NULL || value == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system, component storage, or output is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (index >= particle_system->particle_count) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle index out of range");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    *value = storage[index];
    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProParticleSystem* n_body_sim_pro_particle_system_create(size_t capacity) {
    if (capacity == 0) {
        return NULL;
    }
    if (capacity > SIZE_MAX / (N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT * sizeof(double))) {
        return NULL;
    }

    NBodySimProParticleSystem* particle_system =
        (NBodySimProParticleSystem*)n_body_sim_pro_allocate(
            sizeof(NBodySimProParticleSystem), N_BODY_SIM_PRO_PARTICLE_SYSTEM_ALIGNMENT,
            N_BODY_SIM_PRO_ALLOCATION_CATEGORY_OTHER, __FILE__, __LINE__);
    if (particle_system == NULL) {
        return NULL;
    }
    *particle_system = (NBodySimProParticleSystem){
        .particle_count = 0, .capacity = 0, .positions_x = NULL};

    double** buffers[N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        &particle_system->positions_x, &particle_system->positions_y,
        &particle_system->positions_z, &particle_system->velocities_x,
        &particle_system->velocities_y, &particle_system->velocities_z,
        &particle_system->accelerations_x, &particle_system->accelerations_y,
        &particle_system->accelerations_z, &particle_system->masses};

    size_t component;
    for (component = 0; component < N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        *buffers[component] = (double*)n_body_sim_pro_allocate(
            capacity * sizeof(double), N_BODY_SIM_PRO_PARTICLE_SYSTEM_ALIGNMENT,
            N_BODY_SIM_PRO_ALLOCATION_CATEGORY_PARTICLE_STORAGE, __FILE__, __LINE__);
        if (*buffers[component] == NULL) {
            break;
        }
    }
    if (component < N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT) {
        for (size_t allocated = 0; allocated < component; ++allocated) {
            n_body_sim_pro_deallocate(*buffers[allocated], __FILE__, __LINE__);
        }
        n_body_sim_pro_deallocate(particle_system, __FILE__, __LINE__);
        return NULL;
    }

    particle_system->capacity = capacity;
    return particle_system;
}

void n_body_sim_pro_particle_system_destroy(NBodySimProParticleSystem* particle_system) {
    if (particle_system == NULL) {
        return;
    }
    double* buffers[N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        particle_system->positions_x, particle_system->positions_y,
        particle_system->positions_z, particle_system->velocities_x,
        particle_system->velocities_y, particle_system->velocities_z,
        particle_system->accelerations_x, particle_system->accelerations_y,
        particle_system->accelerations_z, particle_system->masses};
    for (size_t component = 0; component < N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        n_body_sim_pro_deallocate(buffers[component], __FILE__, __LINE__);
    }
    n_body_sim_pro_deallocate(particle_system, __FILE__, __LINE__);
}

size_t n_body_sim_pro_particle_system_particle_count(const NBodySimProParticleSystem* particle_system) {
    return particle_system == NULL ? 0 : particle_system->particle_count;
}

size_t n_body_sim_pro_particle_system_capacity(const NBodySimProParticleSystem* particle_system) {
    return particle_system == NULL ? 0 : particle_system->capacity;
}

NBodySimProStatus n_body_sim_pro_particle_system_reserve(NBodySimProParticleSystem* particle_system,
                                            size_t capacity, NBodySimProError* error) {
    if (particle_system == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (capacity == 0) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "capacity must be non-zero");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (capacity <= particle_system->capacity) {
        return N_BODY_SIM_PRO_STATUS_OK;
    }

    NBodySimProParticleSystem* grown = n_body_sim_pro_particle_system_create(capacity);
    if (grown == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY, __FILE__, __LINE__,
                         "failed to allocate grown particle storage");
        return N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY;
    }

    const size_t count = particle_system->particle_count;
    double* source[N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        particle_system->positions_x, particle_system->positions_y,
        particle_system->positions_z, particle_system->velocities_x,
        particle_system->velocities_y, particle_system->velocities_z,
        particle_system->accelerations_x, particle_system->accelerations_y,
        particle_system->accelerations_z, particle_system->masses};
    double* target[N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT] = {
        grown->positions_x, grown->positions_y, grown->positions_z,
        grown->velocities_x, grown->velocities_y, grown->velocities_z,
        grown->accelerations_x, grown->accelerations_y, grown->accelerations_z,
        grown->masses};
    for (size_t component = 0; component < N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        memcpy(target[component], source[component], count * sizeof(double));
    }
    grown->particle_count = count;

    for (size_t component = 0; component < N_BODY_SIM_PRO_PARTICLE_SYSTEM_COMPONENT_COUNT;
         ++component) {
        n_body_sim_pro_deallocate(source[component], __FILE__, __LINE__);
    }

    *particle_system = *grown;
    n_body_sim_pro_deallocate(grown, __FILE__, __LINE__);
    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProStatus n_body_sim_pro_particle_system_set_particle_count(NBodySimProParticleSystem* particle_system,
                                                       size_t count, NBodySimProError* error) {
    if (particle_system == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (count > particle_system->capacity) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle count exceeds capacity");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    particle_system->particle_count = count;
    return N_BODY_SIM_PRO_STATUS_OK;
}

static NBodySimProStatus require_system(const NBodySimProParticleSystem* particle_system,
                                   NBodySimProError* error) {
    if (particle_system == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProStatus n_body_sim_pro_particle_system_set_position(NBodySimProParticleSystem* particle_system,
                                                 size_t index, NBodySimProVector3 position,
                                                 NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = set_component(particle_system, index,
                                        particle_system->positions_x, position.x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->positions_y,
                           position.y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->positions_z,
                         position.z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_set_velocity(NBodySimProParticleSystem* particle_system,
                                                 size_t index, NBodySimProVector3 velocity,
                                                 NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = set_component(particle_system, index,
                                        particle_system->velocities_x, velocity.x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->velocities_y,
                           velocity.y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->velocities_z,
                         velocity.z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_set_acceleration(NBodySimProParticleSystem* particle_system,
                                                     size_t index, NBodySimProVector3 acceleration,
                                                     NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = set_component(particle_system, index,
                                        particle_system->accelerations_x, acceleration.x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = set_component(particle_system, index, particle_system->accelerations_y,
                           acceleration.y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return set_component(particle_system, index, particle_system->accelerations_z,
                         acceleration.z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_set_mass(NBodySimProParticleSystem* particle_system,
                                             size_t index, double mass, NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    return set_component(particle_system, index, particle_system->masses, mass, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_position(const NBodySimProParticleSystem* particle_system,
                                             size_t index, NBodySimProVector3* position,
                                             NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (position == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output position is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = get_component(particle_system, index,
                                        particle_system->positions_x, &position->x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->positions_y,
                           &position->y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->positions_z,
                         &position->z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_velocity(const NBodySimProParticleSystem* particle_system,
                                             size_t index, NBodySimProVector3* velocity,
                                             NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (velocity == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output velocity is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = get_component(particle_system, index,
                                        particle_system->velocities_x, &velocity->x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->velocities_y,
                           &velocity->y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->velocities_z,
                         &velocity->z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_acceleration(const NBodySimProParticleSystem* particle_system,
                                                 size_t index, NBodySimProVector3* acceleration,
                                                 NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (acceleration == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output acceleration is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    NBodySimProStatus status = get_component(particle_system, index,
                                        particle_system->accelerations_x, &acceleration->x, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    status = get_component(particle_system, index, particle_system->accelerations_y,
                           &acceleration->y, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    return get_component(particle_system, index, particle_system->accelerations_z,
                         &acceleration->z, error);
}

NBodySimProStatus n_body_sim_pro_particle_system_mass(const NBodySimProParticleSystem* particle_system,
                                         size_t index, double* mass, NBodySimProError* error) {
    return get_component(particle_system, index, particle_system->masses, mass, error);
}

#define N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(qualifier, name)                           \
    double* n_body_sim_pro_particle_system_##name(qualifier NBodySimProParticleSystem* system) \
    {                                                                              \
        return system == NULL ? NULL : system->name;                               \
    }

N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, positions_x)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, positions_y)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, positions_z)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, velocities_x)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, velocities_y)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, velocities_z)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, accelerations_x)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, accelerations_y)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, accelerations_z)
N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR(, masses)

#undef N_BODY_SIM_PRO_PARTICLE_SYSTEM_ACCESSOR

NBodySimProStatus n_body_sim_pro_particle_system_view(const NBodySimProParticleSystem* particle_system,
                                         NBodySimProParticleSystemView* view, NBodySimProError* error) {
    if (require_system(particle_system, error) != N_BODY_SIM_PRO_STATUS_OK) {
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (view == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "output view is null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    view->particle_count = particle_system->particle_count;
    view->positions_x = particle_system->positions_x;
    view->positions_y = particle_system->positions_y;
    view->positions_z = particle_system->positions_z;
    view->velocities_x = particle_system->velocities_x;
    view->velocities_y = particle_system->velocities_y;
    view->velocities_z = particle_system->velocities_z;
    view->accelerations_x = particle_system->accelerations_x;
    view->accelerations_y = particle_system->accelerations_y;
    view->accelerations_z = particle_system->accelerations_z;
    view->masses = particle_system->masses;
    return N_BODY_SIM_PRO_STATUS_OK;
}
