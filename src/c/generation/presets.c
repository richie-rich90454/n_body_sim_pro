#include "hpcsim/generation/presets.h"

#include "hpcsim/generation/random.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Unit conventions (documented in docs/physics/presets.md):
 *   G = 1 throughout. Galaxy masses are normalized so the total mass is 1;
 *   disk circular speed V_FLAT = 1; length scales of order unity. These are
 *   educational models, not unit-system claims about real astronomy.
 */

enum { GRAVITATIONAL_CONSTANT_UNITS = 1 };

typedef struct GeneratorContext {
    HpcsimParticleSystem* particle_system;
    HpcsimRandomGenerator random;
    HpcsimError* error;
} GeneratorContext;

const char* hpcsim_preset_string(HpcsimSimulationPreset preset) {
    switch (preset) {
        case HPCSIM_PRESET_TWO_BODY:
            return "two_body";
        case HPCSIM_PRESET_RANDOM_CLOUD:
            return "random_cloud";
        case HPCSIM_PRESET_SOLAR_SYSTEM:
            return "solar_system";
        case HPCSIM_PRESET_OPEN_CLUSTER:
            return "open_cluster";
        case HPCSIM_PRESET_GLOBULAR_CLUSTER:
            return "globular_cluster";
        case HPCSIM_PRESET_SPIRAL_GALAXY:
            return "spiral_galaxy";
        case HPCSIM_PRESET_ELLIPTICAL_GALAXY:
            return "elliptical_galaxy";
        case HPCSIM_PRESET_GALAXY_COLLISION:
            return "galaxy_collision";
        case HPCSIM_PRESET_TRIPLE_GALAXY:
            return "triple_galaxy";
        case HPCSIM_PRESET_COUNT:
            break;
    }
    return "unknown";
}

static int set_particle(GeneratorContext* context, size_t index, HpcsimVector3 position,
                        HpcsimVector3 velocity, double mass) {
    if (hpcsim_particle_system_set_position(context->particle_system, index, position,
                                            context->error) != HPCSIM_STATUS_OK) {
        return 0;
    }
    if (hpcsim_particle_system_set_velocity(context->particle_system, index, velocity,
                                            context->error) != HPCSIM_STATUS_OK) {
        return 0;
    }
    if (hpcsim_particle_system_set_mass(context->particle_system, index, mass,
                                        context->error) != HPCSIM_STATUS_OK) {
        return 0;
    }
    return 1;
}

static int get_velocity(GeneratorContext* context, size_t index, HpcsimVector3* velocity) {
    return hpcsim_particle_system_velocity(context->particle_system, index, velocity,
                                           context->error) == HPCSIM_STATUS_OK;
}

/*
 * Remove the mean velocity of the block [start_index, start_index+count)
 * so the block carries zero net momentum. Requires equal particle masses
 * within the block, which every preset guarantees.
 */
static int remove_mean_velocity(GeneratorContext* context, size_t start_index,
                                size_t count) {
    double mean_x = 0.0;
    double mean_y = 0.0;
    double mean_z = 0.0;
    for (size_t i = 0; i < count; ++i) {
        HpcsimVector3 velocity;
        if (!get_velocity(context, start_index + i, &velocity)) {
            return 0;
        }
        mean_x += velocity.x;
        mean_y += velocity.y;
        mean_z += velocity.z;
    }
    mean_x /= (double)count;
    mean_y /= (double)count;
    mean_z /= (double)count;
    for (size_t i = 0; i < count; ++i) {
        HpcsimVector3 velocity;
        if (!get_velocity(context, start_index + i, &velocity)) {
            return 0;
        }
        velocity.x -= mean_x;
        velocity.y -= mean_y;
        velocity.z -= mean_z;
        if (hpcsim_particle_system_set_velocity(context->particle_system, start_index + i,
                                                velocity, context->error) != HPCSIM_STATUS_OK) {
            return 0;
        }
    }
    return 1;
}

/* Uniform direction on the unit sphere via normalized Gaussians. */
static void random_direction(GeneratorContext* context, double* x, double* y, double* z) {
    for (;;) {
        const double gx = hpcsim_random_next_gaussian(&context->random);
        const double gy = hpcsim_random_next_gaussian(&context->random);
        const double gz = hpcsim_random_next_gaussian(&context->random);
        const double norm = sqrt(gx * gx + gy * gy + gz * gz);
        if (norm > 1.0e-12) {
            *x = gx / norm;
            *y = gy / norm;
            *z = gz / norm;
            return;
        }
    }
}

/* Uniform point inside a sphere of radius `radius`. */
static void random_point_in_sphere(GeneratorContext* context, double radius, double* x,
                                   double* y, double* z) {
    const double direction_cube_root = cbrt(hpcsim_random_next_double(&context->random));
    const double distance = radius * direction_cube_root;
    double direction_x;
    double direction_y;
    double direction_z;
    random_direction(context, &direction_x, &direction_y, &direction_z);
    *x = distance * direction_x;
    *y = distance * direction_y;
    *z = distance * direction_z;
}

/* Radius from the Plummer profile: r = a / sqrt(u^(-2/3) - 1). */
static double plummer_radius(GeneratorContext* context, double scale_radius) {
    const double u = 1.0 - hpcsim_random_next_double(&context->random);
    return scale_radius / sqrt(pow(u, -2.0 / 3.0) - 1.0);
}

/* Isotropic velocity dispersion for a Plummer model at radius `r`. */
static double plummer_velocity_dispersion(double scale_radius, double radius,
                                          double total_mass) {
    const double denominator = sqrt(scale_radius * scale_radius + radius * radius);
    return sqrt(GRAVITATIONAL_CONSTANT_UNITS * total_mass / (6.0 * denominator));
}

/* Radius from the 2D exponential disk profile (Gamma(2, scale_radius)). */
static double exponential_disk_radius(GeneratorContext* context, double scale_radius) {
    const double e1 = -log(1.0 - hpcsim_random_next_double(&context->random));
    const double e2 = -log(1.0 - hpcsim_random_next_double(&context->random));
    return scale_radius * (e1 + e2);
}

/*
 * Generate a Plummer sphere (globular/elliptical star cluster) in place.
 * Positions come from the analytic Plummer profile; velocities are sampled
 * from the isotropic Gaussian with the Plummer velocity dispersion. The mean
 * velocity is removed in a second pass so the cluster has zero net momentum.
 * `center` may be NULL for a sphere about the origin.
 */
static int generate_plummer_sphere(GeneratorContext* context, size_t start_index,
                                   size_t particle_count, double scale_radius,
                                   double total_mass, const HpcsimVector3* center) {
    const double particle_mass = total_mass / (double)particle_count;

    for (size_t i = 0; i < particle_count; ++i) {
        const double radius = plummer_radius(context, scale_radius);
        double direction_x;
        double direction_y;
        double direction_z;
        random_direction(context, &direction_x, &direction_y, &direction_z);
        HpcsimVector3 position = {radius * direction_x, radius * direction_y,
                                  radius * direction_z};
        if (center != NULL) {
            position.x += center->x;
            position.y += center->y;
            position.z += center->z;
        }

        const double dispersion =
            plummer_velocity_dispersion(scale_radius, radius, total_mass);
        const HpcsimVector3 velocity = {
            hpcsim_random_next_gaussian(&context->random) * dispersion,
            hpcsim_random_next_gaussian(&context->random) * dispersion,
            hpcsim_random_next_gaussian(&context->random) * dispersion};

        if (!set_particle(context, start_index + i, position, velocity, particle_mass)) {
            return 0;
        }
    }

    return remove_mean_velocity(context, start_index, particle_count);
}

/*
 * Generate an exponential disk galaxy in place, with a flat rotation curve
 * and a small isotropic velocity dispersion. `arm_twist` > 0 modulates the
 * azimuthal density into `arm_count` trailing spiral arms; 0 produces a
 * smooth disk. The mean velocity is removed so each generated block has zero
 * net momentum.
 */
static int generate_disk_galaxy(GeneratorContext* context, size_t start_index,
                                size_t particle_count, double scale_radius,
                                double disk_scale_height, double circular_speed,
                                double velocity_dispersion, double arm_twist,
                                int arm_count, const HpcsimVector3* center,
                                double particle_mass) {
    for (size_t i = 0; i < particle_count; ++i) {
        const double radius = exponential_disk_radius(context, scale_radius);
        double angle = 2.0 * M_PI * hpcsim_random_next_double(&context->random);

        if (arm_twist > 0.0 && arm_count > 0) {
            const double arm_index =
                hpcsim_random_next_double(&context->random) * (double)arm_count;
            const double arm_angle = 2.0 * M_PI * arm_index / (double)arm_count +
                                     arm_twist * radius / scale_radius;
            const double arm_width = 0.15;
            angle = arm_angle + hpcsim_random_next_gaussian(&context->random) * arm_width;
        }

        const double height = hpcsim_random_next_gaussian(&context->random) * disk_scale_height;

        const double disk_x = radius * cos(angle);
        const double disk_y = radius * sin(angle);

        const double tangential_x = -sin(angle);
        const double tangential_y = cos(angle);

        const HpcsimVector3 position = {disk_x, disk_y, height};
        const HpcsimVector3 velocity = {
            circular_speed * tangential_x +
                hpcsim_random_next_gaussian(&context->random) * velocity_dispersion,
            circular_speed * tangential_y +
                hpcsim_random_next_gaussian(&context->random) * velocity_dispersion,
            hpcsim_random_next_gaussian(&context->random) * velocity_dispersion * 0.5};

        HpcsimVector3 final_position = position;
        if (center != NULL) {
            final_position.x += center->x;
            final_position.y += center->y;
            final_position.z += center->z;
        }
        if (!set_particle(context, start_index + i, final_position, velocity,
                          particle_mass)) {
            return 0;
        }
    }

    return remove_mean_velocity(context, start_index, particle_count);
}

static int generate_two_body(GeneratorContext* context, HpcsimError* error) {
    (void)error;
    const HpcsimVector3 position_0 = {-0.5, 0.0, 0.0};
    const HpcsimVector3 position_1 = {0.5, 0.0, 0.0};
    const double per_body_speed = sqrt(2.0) / 2.0;
    const HpcsimVector3 velocity_0 = {0.0, per_body_speed, 0.0};
    const HpcsimVector3 velocity_1 = {0.0, -per_body_speed, 0.0};
    return set_particle(context, 0, position_0, velocity_0, 1.0) &&
           set_particle(context, 1, position_1, velocity_1, 1.0);
}

static int generate_random_cloud(GeneratorContext* context, size_t particle_count) {
    const double particle_mass = 1.0 / (double)particle_count;
    for (size_t i = 0; i < particle_count; ++i) {
        double x;
        double y;
        double z;
        random_point_in_sphere(context, 1.0, &x, &y, &z);
        const HpcsimVector3 velocity = {
            hpcsim_random_next_double_range(&context->random, -0.05, 0.05),
            hpcsim_random_next_double_range(&context->random, -0.05, 0.05),
            hpcsim_random_next_double_range(&context->random, -0.05, 0.05)};
        if (!set_particle(context, i, (HpcsimVector3){x, y, z}, velocity, particle_mass)) {
            return 0;
        }
    }
    return remove_mean_velocity(context, 0, particle_count);
}

static int generate_open_cluster(GeneratorContext* context, size_t particle_count) {
    const double particle_mass = 1.0 / (double)particle_count;
    const double position_dispersion = 0.3;
    const double velocity_dispersion = 0.02;
    for (size_t i = 0; i < particle_count; ++i) {
        const HpcsimVector3 position = {
            hpcsim_random_next_gaussian(&context->random) * position_dispersion,
            hpcsim_random_next_gaussian(&context->random) * position_dispersion,
            hpcsim_random_next_gaussian(&context->random) * position_dispersion};
        const HpcsimVector3 velocity = {
            hpcsim_random_next_gaussian(&context->random) * velocity_dispersion,
            hpcsim_random_next_gaussian(&context->random) * velocity_dispersion,
            hpcsim_random_next_gaussian(&context->random) * velocity_dispersion};
        if (!set_particle(context, i, position, velocity, particle_mass)) {
            return 0;
        }
    }
    return remove_mean_velocity(context, 0, particle_count);
}

static int generate_solar_system(GeneratorContext* context, size_t planet_count,
                                 HpcsimError* error) {
    if (planet_count < 1) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "solar system preset needs at least one planet");
        return 0;
    }
    const double stellar_mass = 1.0;
    if (!set_particle(context, 0, (HpcsimVector3){0, 0, 0}, (HpcsimVector3){0, 0, 0},
                      stellar_mass)) {
        return 0;
    }
    for (size_t i = 0; i < planet_count; ++i) {
        const double radius = 1.0 + 3.0 * (double)i / (double)planet_count;
        const double circular_speed =
            sqrt(GRAVITATIONAL_CONSTANT_UNITS * stellar_mass / radius);
        const double angle = 2.0 * M_PI * hpcsim_random_next_double(&context->random);
        const double planet_mass = 0.001;
        const HpcsimVector3 position = {radius * cos(angle), radius * sin(angle), 0.0};
        const HpcsimVector3 velocity = {-circular_speed * sin(angle),
                                        circular_speed * cos(angle), 0.0};
        if (!set_particle(context, i + 1, position, velocity, planet_mass)) {
            return 0;
        }
    }
    return 1;
}

/*
 * Two exponential disks approaching head-on along the x axis. Each galaxy
 * is a virialized-looking disk; equal masses and equal-and-opposite approach
 * speeds keep the total momentum at zero.
 */
static int generate_galaxy_collision(GeneratorContext* context, size_t particle_count) {
    const size_t half = particle_count / 2;
    const double galaxy_mass = 0.5;
    const double particle_mass = galaxy_mass / (double)half;
    const HpcsimVector3 left_center = {-2.5, 0.0, 0.0};
    const HpcsimVector3 right_center = {2.5, 0.0, 0.0};
    const double approach_speed = 0.35;

    if (!generate_disk_galaxy(context, 0, half, 0.7, 0.1, 1.0, 0.12, 0.0, 0, &left_center,
                              particle_mass)) {
        return 0;
    }
    if (!generate_disk_galaxy(context, half, half, 0.7, 0.1, 1.0, 0.12, 0.0, 0,
                              &right_center, particle_mass)) {
        return 0;
    }

    for (size_t i = 0; i < particle_count; ++i) {
        HpcsimVector3 velocity;
        if (!get_velocity(context, i, &velocity)) {
            return 0;
        }
        velocity.x += i < half ? approach_speed : -approach_speed;
        if (hpcsim_particle_system_set_velocity(context->particle_system, i, velocity,
                                                context->error) != HPCSIM_STATUS_OK) {
            return 0;
        }
    }
    return 1;
}

static int generate_triple_galaxy(GeneratorContext* context, size_t particle_count) {
    const size_t third = particle_count / 3;
    const double galaxy_mass = 1.0 / 3.0;
    const HpcsimVector3 centers[3] = {{-1.8, 0.0, 0.0}, {1.8, 0.0, 0.0}, {0.0, 1.8, 0.0}};
    for (size_t galaxy = 0; galaxy < 3; ++galaxy) {
        if (!generate_plummer_sphere(context, galaxy * third, third, 0.7, galaxy_mass,
                                     &centers[galaxy])) {
            return 0;
        }
    }
    return 1;
}

HpcsimStatus hpcsim_preset_generate(HpcsimParticleSystem* particle_system,
                                    HpcsimSimulationPreset preset,
                                    const HpcsimPresetParameters* parameters,
                                    HpcsimError* error) {
    if (particle_system == NULL || parameters == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system and parameters must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (parameters->particle_count == 0) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle count must be non-zero");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }
    if (hpcsim_particle_system_capacity(particle_system) < parameters->particle_count) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system capacity is smaller than requested count");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    GeneratorContext context;
    context.particle_system = particle_system;
    context.error = error;
    hpcsim_random_init(&context.random, parameters->random_seed);

    HpcsimStatus status = hpcsim_particle_system_set_particle_count(
        particle_system, parameters->particle_count, error);
    if (status != HPCSIM_STATUS_OK) {
        return status;
    }

    int generated = 0;
    switch (preset) {
        case HPCSIM_PRESET_TWO_BODY:
            if (parameters->particle_count != 2) {
                hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                                 "two_body preset requires exactly 2 particles");
                return HPCSIM_STATUS_INVALID_ARGUMENT;
            }
            generated = generate_two_body(&context, error);
            break;
        case HPCSIM_PRESET_RANDOM_CLOUD:
            generated = generate_random_cloud(&context, parameters->particle_count);
            break;
        case HPCSIM_PRESET_SOLAR_SYSTEM:
            generated = generate_solar_system(&context, parameters->particle_count - 1, error);
            break;
        case HPCSIM_PRESET_OPEN_CLUSTER:
            generated = generate_open_cluster(&context, parameters->particle_count);
            break;
        case HPCSIM_PRESET_GLOBULAR_CLUSTER:
            generated = generate_plummer_sphere(&context, 0, parameters->particle_count,
                                                1.0, 1.0, NULL);
            break;
        case HPCSIM_PRESET_ELLIPTICAL_GALAXY:
            generated = generate_plummer_sphere(&context, 0, parameters->particle_count,
                                                2.0, 1.0, NULL);
            break;
        case HPCSIM_PRESET_SPIRAL_GALAXY: {
            const double particle_mass = 1.0 / (double)parameters->particle_count;
            generated = generate_disk_galaxy(&context, 0, parameters->particle_count, 0.7,
                                             0.1, 1.0, 0.12, 3.5, 2, NULL, particle_mass);
            break;
        }
        case HPCSIM_PRESET_GALAXY_COLLISION:
            generated = generate_galaxy_collision(&context, parameters->particle_count);
            break;
        case HPCSIM_PRESET_TRIPLE_GALAXY:
            generated = generate_triple_galaxy(&context, parameters->particle_count);
            break;
        case HPCSIM_PRESET_COUNT:
            break;
    }

    if (!generated) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_STATE, __FILE__, __LINE__,
                         "preset generation failed");
        return hpcsim_error_failed(error) ? error->status : HPCSIM_STATUS_INVALID_STATE;
    }

    return HPCSIM_STATUS_OK;
}
