#include "hpcsim/physics/gravity.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <math.h>

/*
 * OpenMP-parallel O(N^2) all-pairs acceleration kernel.
 *
 * The outer loop over i is distributed across threads with a static
 * schedule. Each thread writes only the accelerations of the particles it
 * owns, so there is no write contention. The inner force sum per particle is
 * unchanged from the reference kernel, which makes the results bit-identical
 * to the reference on a given machine: parallelizing this way is exact, not
 * an approximation, and preserves determinism.
 *
 * The reference kernel remains the correctness authority; this kernel is
 * validated against it in the test suite.
 */

#ifndef _OPENMP
HpcsimStatus hpcsim_gravity_compute_acceleration_openmp(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    return hpcsim_gravity_compute_acceleration_reference(view, gravity, error);
}
#else
HpcsimStatus hpcsim_gravity_compute_acceleration_openmp(
    const HpcsimParticleSystemView* view, const HpcsimGravity* gravity, HpcsimError* error) {
    if (view == NULL || gravity == NULL) {
        hpcsim_error_set(error, HPCSIM_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view and gravity parameters must not be null");
        return HPCSIM_STATUS_INVALID_ARGUMENT;
    }

    const size_t particle_count = view->particle_count;
    const double* const positions_x = view->positions_x;
    const double* const positions_y = view->positions_y;
    const double* const positions_z = view->positions_z;
    const double* const masses = view->masses;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    const double gravitational_constant = gravity->gravitational_constant;
    const double softening_squared = gravity->softening_squared;

#pragma omp parallel for schedule(static)
    for (long long i = 0; i < (long long)particle_count; ++i) {
        double acceleration_x = 0.0;
        double acceleration_y = 0.0;
        double acceleration_z = 0.0;

        for (size_t j = 0; j < particle_count; ++j) {
            if (j == (size_t)i) {
                continue;
            }
            const double delta_x = positions_x[j] - positions_x[i];
            const double delta_y = positions_y[j] - positions_y[i];
            const double delta_z = positions_z[j] - positions_z[i];

            const double distance_squared =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z + softening_squared;
            const double inverse_distance = 1.0 / sqrt(distance_squared);
            const double inverse_distance_cubed =
                inverse_distance * inverse_distance * inverse_distance;

            const double force_scale =
                gravitational_constant * masses[j] * inverse_distance_cubed;

            acceleration_x += force_scale * delta_x;
            acceleration_y += force_scale * delta_y;
            acceleration_z += force_scale * delta_z;
        }

        accelerations_x[i] = acceleration_x;
        accelerations_y[i] = acceleration_y;
        accelerations_z[i] = acceleration_z;
    }

    return HPCSIM_STATUS_OK;
}
#endif
