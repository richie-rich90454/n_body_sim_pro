#include "n_body_sim_pro/generation/presets.h"

#include "n_body_sim_pro/generation/random.h"
#include "n_body_sim_pro/threading/numa.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * Parallel, NUMA-aware preset generation.
 *
 * Each particle is filled independently: its pseudo-random numbers come from
 * a per-particle generator seeded deterministically from (master_seed,
 * particle index), so the output is reproducible for any thread count.
 * Threads write disjoint slices of the SoA arrays, which is both a parallel
 * generation and a first-touch placement: pages are established on the NUMA
 * node of the thread that will later process them.
 *
 * Per-preset math mirrors docs/physics/presets.md. The sequential generator
 * (n_body_sim_pro_preset_generate) remains the correctness reference; this variant
 * is used for large systems where parallel generation and page placement
 * pay off.
 */

enum { GRAVITATIONAL_CONSTANT_UNITS = 1 };

static uint64_t mix_seed(uint64_t master_seed, size_t particle_index) {
    uint64_t x = master_seed ^ ((uint64_t)particle_index + 0x9E3779B97F4A7C15ull);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

static void fill_two_body(NBodySimProParticleSystem* particle_system, NBodySimProError* error) {
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
    const double per_body_speed = sqrt(2.0) / 2.0;
    px[0] = -0.5; py[0] = 0.0; pz[0] = 0.0;
    vx[0] = 0.0; vy[0] = per_body_speed; vz[0] = 0.0; mass[0] = 1.0;
    px[1] = 0.5; py[1] = 0.0; pz[1] = 0.0;
    vx[1] = 0.0; vy[1] = -per_body_speed; vz[1] = 0.0; mass[1] = 1.0;
    (void)error;
}

static void fill_solar_system(NBodySimProParticleSystem* particle_system, uint64_t seed,
                              NBodySimProError* error) {
    const size_t count = n_body_sim_pro_particle_system_particle_count(particle_system);
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
    const size_t planet_count = count - 1;
    NBodySimProRandomGenerator generator;
    n_body_sim_pro_random_init(&generator, seed);
    px[0] = 0.0; py[0] = 0.0; pz[0] = 0.0;
    vx[0] = 0.0; vy[0] = 0.0; vz[0] = 0.0; mass[0] = 1.0;
    for (size_t i = 0; i < planet_count; ++i) {
        const double radius = 1.0 + 3.0 * (double)i / (double)planet_count;
        const double circular_speed = sqrt(GRAVITATIONAL_CONSTANT_UNITS / radius);
        const double angle = 2.0 * M_PI * n_body_sim_pro_random_next_double(&generator);
        const size_t index = i + 1;
        px[index] = radius * cos(angle);
        py[index] = radius * sin(angle);
        pz[index] = 0.0;
        vx[index] = -circular_speed * sin(angle);
        vy[index] = circular_speed * cos(angle);
        vz[index] = 0.0;
        mass[index] = 0.001;
    }
    (void)error;
}

/* Plummer-model fill for a block of particles. */
static void fill_plummer_block(NBodySimProParticleSystem* particle_system, uint64_t master_seed,
                               size_t start, size_t count, double scale_radius,
                               double total_mass) {
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
    const double particle_mass = total_mass / (double)count;

    double mean_vx = 0.0;
    double mean_vy = 0.0;
    double mean_vz = 0.0;
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t i = start + offset;
        NBodySimProRandomGenerator generator;
        n_body_sim_pro_random_init(&generator, mix_seed(master_seed, i));
        const double u = 1.0 - n_body_sim_pro_random_next_double(&generator);
        const double radius = scale_radius / sqrt(pow(u, -2.0 / 3.0) - 1.0);
        double direction_x;
        double direction_y;
        double direction_z;
        for (;;) {
            const double gx = n_body_sim_pro_random_next_gaussian(&generator);
            const double gy = n_body_sim_pro_random_next_gaussian(&generator);
            const double gz = n_body_sim_pro_random_next_gaussian(&generator);
            const double norm = sqrt(gx * gx + gy * gy + gz * gz);
            if (norm > 1.0e-12) {
                direction_x = gx / norm;
                direction_y = gy / norm;
                direction_z = gz / norm;
                break;
            }
        }
        px[i] = radius * direction_x;
        py[i] = radius * direction_y;
        pz[i] = radius * direction_z;

        const double dispersion =
            sqrt(GRAVITATIONAL_CONSTANT_UNITS * total_mass /
                 (6.0 * sqrt(scale_radius * scale_radius + radius * radius)));
        const double v_x = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
        const double v_y = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
        const double v_z = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
        vx[i] = v_x;
        vy[i] = v_y;
        vz[i] = v_z;
        mean_vx += v_x;
        mean_vy += v_y;
        mean_vz += v_z;
        mass[i] = particle_mass;
    }
    mean_vx /= (double)count;
    mean_vy /= (double)count;
    mean_vz /= (double)count;
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t i = start + offset;
        vx[i] -= mean_vx;
        vy[i] -= mean_vy;
        vz[i] -= mean_vz;
    }
}

/* Exponential-disk fill for a block of particles. */
static void fill_disk_block(NBodySimProParticleSystem* particle_system, uint64_t master_seed,
                            size_t start, size_t count, double scale_radius,
                            double disk_scale_height, double circular_speed,
                            double velocity_dispersion, double arm_twist, int arm_count,
                            double particle_mass) {
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);

    double mean_vx = 0.0;
    double mean_vy = 0.0;
    double mean_vz = 0.0;
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t i = start + offset;
        NBodySimProRandomGenerator generator;
        n_body_sim_pro_random_init(&generator, mix_seed(master_seed, i));
        const double e1 = -log(1.0 - n_body_sim_pro_random_next_double(&generator));
        const double e2 = -log(1.0 - n_body_sim_pro_random_next_double(&generator));
        const double radius = scale_radius * (e1 + e2);
        double angle = 2.0 * M_PI * n_body_sim_pro_random_next_double(&generator);
        if (arm_twist > 0.0 && arm_count > 0) {
            const double arm_index =
                n_body_sim_pro_random_next_double(&generator) * (double)arm_count;
            const double arm_angle = 2.0 * M_PI * arm_index / (double)arm_count +
                                     arm_twist * radius / scale_radius;
            angle = arm_angle + n_body_sim_pro_random_next_gaussian(&generator) * 0.15;
        }
        const double height = n_body_sim_pro_random_next_gaussian(&generator) * disk_scale_height;
        px[i] = radius * cos(angle);
        py[i] = radius * sin(angle);
        pz[i] = height;
        const double tangential_x = -sin(angle);
        const double tangential_y = cos(angle);
        const double v_x = circular_speed * tangential_x +
                           n_body_sim_pro_random_next_gaussian(&generator) * velocity_dispersion;
        const double v_y = circular_speed * tangential_y +
                           n_body_sim_pro_random_next_gaussian(&generator) * velocity_dispersion;
        const double v_z =
            n_body_sim_pro_random_next_gaussian(&generator) * velocity_dispersion * 0.5;
        vx[i] = v_x;
        vy[i] = v_y;
        vz[i] = v_z;
        mean_vx += v_x;
        mean_vy += v_y;
        mean_vz += v_z;
        mass[i] = particle_mass;
    }
    mean_vx /= (double)count;
    mean_vy /= (double)count;
    mean_vz /= (double)count;
    for (size_t offset = 0; offset < count; ++offset) {
        const size_t i = start + offset;
        vx[i] -= mean_vx;
        vy[i] -= mean_vy;
        vz[i] -= mean_vz;
    }
}

static void fill_random_cloud(NBodySimProParticleSystem* particle_system, uint64_t seed) {
    const size_t count = n_body_sim_pro_particle_system_particle_count(particle_system);
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
    const double particle_mass = 1.0 / (double)count;

    double mean_vx = 0.0;
    double mean_vy = 0.0;
    double mean_vz = 0.0;
#pragma omp parallel for schedule(static) reduction(+ : mean_vx, mean_vy, mean_vz)
    for (long long i = 0; i < (long long)count; ++i) {
        NBodySimProRandomGenerator generator;
        n_body_sim_pro_random_init(&generator, mix_seed(seed, (size_t)i));
        double direction_x;
        double direction_y;
        double direction_z;
        for (;;) {
            const double gx = n_body_sim_pro_random_next_gaussian(&generator);
            const double gy = n_body_sim_pro_random_next_gaussian(&generator);
            const double gz = n_body_sim_pro_random_next_gaussian(&generator);
            const double norm = sqrt(gx * gx + gy * gy + gz * gz);
            if (norm > 1.0e-12) {
                direction_x = gx / norm;
                direction_y = gy / norm;
                direction_z = gz / norm;
                break;
            }
        }
        const double radius = cbrt(n_body_sim_pro_random_next_double(&generator));
        px[i] = radius * direction_x;
        py[i] = radius * direction_y;
        pz[i] = radius * direction_z;
        const double v_x = n_body_sim_pro_random_next_double_range(&generator, -0.05, 0.05);
        const double v_y = n_body_sim_pro_random_next_double_range(&generator, -0.05, 0.05);
        const double v_z = n_body_sim_pro_random_next_double_range(&generator, -0.05, 0.05);
        vx[i] = v_x;
        vy[i] = v_y;
        vz[i] = v_z;
        mass[i] = particle_mass;
        mean_vx += v_x;
        mean_vy += v_y;
        mean_vz += v_z;
    }
    mean_vx /= (double)count;
    mean_vy /= (double)count;
    mean_vz /= (double)count;
#pragma omp parallel for schedule(static)
    for (long long i = 0; i < (long long)count; ++i) {
        vx[i] -= mean_vx;
        vy[i] -= mean_vy;
        vz[i] -= mean_vz;
    }
}

static void fill_open_cluster(NBodySimProParticleSystem* particle_system, uint64_t seed) {
    const size_t count = n_body_sim_pro_particle_system_particle_count(particle_system);
    double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
    double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
    double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
    const double particle_mass = 1.0 / (double)count;

    double mean_vx = 0.0;
    double mean_vy = 0.0;
    double mean_vz = 0.0;
    for (size_t i = 0; i < count; ++i) {
        NBodySimProRandomGenerator generator;
        n_body_sim_pro_random_init(&generator, mix_seed(seed, i));
        px[i] = n_body_sim_pro_random_next_gaussian(&generator) * 0.3;
        py[i] = n_body_sim_pro_random_next_gaussian(&generator) * 0.3;
        pz[i] = n_body_sim_pro_random_next_gaussian(&generator) * 0.3;
        const double v_x = n_body_sim_pro_random_next_gaussian(&generator) * 0.02;
        const double v_y = n_body_sim_pro_random_next_gaussian(&generator) * 0.02;
        const double v_z = n_body_sim_pro_random_next_gaussian(&generator) * 0.02;
        vx[i] = v_x;
        vy[i] = v_y;
        vz[i] = v_z;
        mass[i] = particle_mass;
        mean_vx += v_x;
        mean_vy += v_y;
        mean_vz += v_z;
    }
    mean_vx /= (double)count;
    mean_vy /= (double)count;
    mean_vz /= (double)count;
    for (size_t i = 0; i < count; ++i) {
        vx[i] -= mean_vx;
        vy[i] -= mean_vy;
        vz[i] -= mean_vz;
    }
}

NBodySimProStatus n_body_sim_pro_preset_generate_parallel(NBodySimProParticleSystem* particle_system,
                                             NBodySimProSimulationPreset preset,
                                             const NBodySimProPresetParameters* parameters,
                                             NBodySimProError* error) {
    if (particle_system == NULL || parameters == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system and parameters must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (n_body_sim_pro_particle_system_capacity(particle_system) < parameters->particle_count) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "particle system capacity is smaller than requested count");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    NBodySimProStatus status = n_body_sim_pro_particle_system_set_particle_count(
        particle_system, parameters->particle_count, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }
    const size_t count = parameters->particle_count;
    const uint64_t seed = parameters->random_seed;

    /* First-touch the SoA arrays before generation so their pages are
     * established on the node of the touching thread; the generation pass
     * then writes onto those resident pages. Writing 0.0 to one element per
     * page is the touch; generation overwrites every element afterwards. */
    double* const first_touch_arrays[10] = {
        n_body_sim_pro_particle_system_positions_x(particle_system),
        n_body_sim_pro_particle_system_positions_y(particle_system),
        n_body_sim_pro_particle_system_positions_z(particle_system),
        n_body_sim_pro_particle_system_velocities_x(particle_system),
        n_body_sim_pro_particle_system_velocities_y(particle_system),
        n_body_sim_pro_particle_system_velocities_z(particle_system),
        n_body_sim_pro_particle_system_accelerations_x(particle_system),
        n_body_sim_pro_particle_system_accelerations_y(particle_system),
        n_body_sim_pro_particle_system_accelerations_z(particle_system),
        n_body_sim_pro_particle_system_masses(particle_system)};
#pragma omp parallel for schedule(static)
    for (int array = 0; array < 10; ++array) {
        n_body_sim_pro_memory_first_touch(first_touch_arrays[array], count);
    }

    switch (preset) {
        case N_BODY_SIM_PRO_PRESET_TWO_BODY:
            if (count != 2) {
                n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                                 "two_body preset requires exactly 2 particles");
                return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
            }
            fill_two_body(particle_system, error);
            break;
        case N_BODY_SIM_PRO_PRESET_SOLAR_SYSTEM:
            fill_solar_system(particle_system, seed, error);
            break;
        case N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD:
            fill_random_cloud(particle_system, seed);
            break;
        case N_BODY_SIM_PRO_PRESET_OPEN_CLUSTER:
            fill_open_cluster(particle_system, seed);
            break;
        case N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER:
        case N_BODY_SIM_PRO_PRESET_ELLIPTICAL_GALAXY: {
            const double scale_radius = preset == N_BODY_SIM_PRO_PRESET_GLOBULAR_CLUSTER ? 1.0 : 2.0;
            fill_plummer_block(particle_system, seed, 0, count, scale_radius, 1.0);
            break;
        }
        case N_BODY_SIM_PRO_PRESET_SPIRAL_GALAXY: {
            const double particle_mass = 1.0 / (double)count;
            fill_disk_block(particle_system, seed, 0, count, 0.7, 0.1, 1.0, 0.12, 3.5, 2,
                            particle_mass);
            break;
        }
        case N_BODY_SIM_PRO_PRESET_GALAXY_COLLISION: {
            const size_t half = count / 2;
            const double particle_mass = 0.5 / (double)half;
            fill_disk_block(particle_system, mix_seed(seed, 0), 0, half, 0.7, 0.1, 1.0,
                            0.12, 0.0, 0, particle_mass);
            fill_disk_block(particle_system, mix_seed(seed, 1), half, half, 0.7, 0.1, 1.0,
                            0.12, 0.0, 0, particle_mass);
            double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
            const double approach_speed = 0.35;
            for (size_t i = 0; i < count; ++i) {
                vx[i] += i < half ? approach_speed : -approach_speed;
            }
            break;
        }
        case N_BODY_SIM_PRO_PRESET_TRIPLE_GALAXY: {
            const size_t third = count / 3;
            const double galaxy_mass = 1.0 / 3.0;
            const double centers[3][3] = {{-1.8, 0.0, 0.0}, {1.8, 0.0, 0.0},
                                          {0.0, 1.8, 0.0}};
            for (int galaxy = 0; galaxy < 3; ++galaxy) {
                double* const px = n_body_sim_pro_particle_system_positions_x(particle_system);
                double* const py = n_body_sim_pro_particle_system_positions_y(particle_system);
                double* const pz = n_body_sim_pro_particle_system_positions_z(particle_system);
                const size_t start = (size_t)galaxy * third;
                double mean_vx = 0.0;
                double mean_vy = 0.0;
                double mean_vz = 0.0;
                for (size_t offset = 0; offset < third; ++offset) {
                    const size_t i = start + offset;
                    NBodySimProRandomGenerator generator;
                    n_body_sim_pro_random_init(&generator, mix_seed(mix_seed(seed, (size_t)galaxy),
                                                            i));
                    const double u = 1.0 - n_body_sim_pro_random_next_double(&generator);
                    const double radius = 0.7 / sqrt(pow(u, -2.0 / 3.0) - 1.0);
                    double direction_x;
                    double direction_y;
                    double direction_z;
                    for (;;) {
                        const double gx = n_body_sim_pro_random_next_gaussian(&generator);
                        const double gy = n_body_sim_pro_random_next_gaussian(&generator);
                        const double gz = n_body_sim_pro_random_next_gaussian(&generator);
                        const double norm = sqrt(gx * gx + gy * gy + gz * gz);
                        if (norm > 1.0e-12) {
                            direction_x = gx / norm;
                            direction_y = gy / norm;
                            direction_z = gz / norm;
                            break;
                        }
                    }
                    px[i] = radius * direction_x + centers[galaxy][0];
                    py[i] = radius * direction_y + centers[galaxy][1];
                    pz[i] = radius * direction_z + centers[galaxy][2];
                    const double dispersion =
                        sqrt(GRAVITATIONAL_CONSTANT_UNITS * galaxy_mass /
                             (6.0 * sqrt(0.49 + radius * radius)));
                    double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
                    double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
                    double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
                    double* const mass = n_body_sim_pro_particle_system_masses(particle_system);
                    const double v_x = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
                    const double v_y = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
                    const double v_z = n_body_sim_pro_random_next_gaussian(&generator) * dispersion;
                    vx[i] = v_x;
                    vy[i] = v_y;
                    vz[i] = v_z;
                    mass[i] = galaxy_mass / (double)third;
                    mean_vx += v_x;
                    mean_vy += v_y;
                    mean_vz += v_z;
                }
                mean_vx /= (double)third;
                mean_vy /= (double)third;
                mean_vz /= (double)third;
                double* const vx = n_body_sim_pro_particle_system_velocities_x(particle_system);
                double* const vy = n_body_sim_pro_particle_system_velocities_y(particle_system);
                double* const vz = n_body_sim_pro_particle_system_velocities_z(particle_system);
                for (size_t offset = 0; offset < third; ++offset) {
                    const size_t i = start + offset;
                    vx[i] -= mean_vx;
                    vy[i] -= mean_vy;
                    vz[i] -= mean_vz;
                }
            }
            break;
        }
        case N_BODY_SIM_PRO_PRESET_COUNT:
            break;
    }

    return N_BODY_SIM_PRO_STATUS_OK;
}
