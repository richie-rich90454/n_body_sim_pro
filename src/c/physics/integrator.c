#include "n_body_sim_pro/physics/integrator.h"

const char* n_body_sim_pro_integrator_type_string(NBodySimProIntegratorType integrator) {
    switch (integrator) {
        case N_BODY_SIM_PRO_INTEGRATOR_EULER:
            return "euler";
        case N_BODY_SIM_PRO_INTEGRATOR_LEAPFROG:
            return "leapfrog";
        case N_BODY_SIM_PRO_INTEGRATOR_VELOCITY_VERLET:
            return "velocity_verlet";
    }
    return "unknown";
}

static NBodySimProStatus advance_euler(NBodySimProParticleSystemView* view,
                                  const NBodySimProGravity* gravity, double timestep,
                                  NBodySimProForceFunction force_function,
                                  void* force_context, NBodySimProError* error) {
    NBodySimProStatus status = force_function(view, gravity, force_context, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }

    const size_t particle_count = view->particle_count;
    double* const positions_x = view->positions_x;
    double* const positions_y = view->positions_y;
    double* const positions_z = view->positions_z;
    double* const velocities_x = view->velocities_x;
    double* const velocities_y = view->velocities_y;
    double* const velocities_z = view->velocities_z;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    for (size_t i = 0; i < particle_count; ++i) {
        positions_x[i] += velocities_x[i] * timestep;
        positions_y[i] += velocities_y[i] * timestep;
        positions_z[i] += velocities_z[i] * timestep;

        velocities_x[i] += accelerations_x[i] * timestep;
        velocities_y[i] += accelerations_y[i] * timestep;
        velocities_z[i] += accelerations_z[i] * timestep;
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

static NBodySimProStatus advance_leapfrog(NBodySimProParticleSystemView* view,
                                     const NBodySimProGravity* gravity, double timestep,
                                     NBodySimProForceFunction force_function,
                                     void* force_context, NBodySimProError* error) {
    const size_t particle_count = view->particle_count;
    double* const positions_x = view->positions_x;
    double* const positions_y = view->positions_y;
    double* const positions_z = view->positions_z;
    double* const velocities_x = view->velocities_x;
    double* const velocities_y = view->velocities_y;
    double* const velocities_z = view->velocities_z;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    const double half_timestep = 0.5 * timestep;

    for (size_t i = 0; i < particle_count; ++i) {
        velocities_x[i] += accelerations_x[i] * half_timestep;
        velocities_y[i] += accelerations_y[i] * half_timestep;
        velocities_z[i] += accelerations_z[i] * half_timestep;

        positions_x[i] += velocities_x[i] * timestep;
        positions_y[i] += velocities_y[i] * timestep;
        positions_z[i] += velocities_z[i] * timestep;
    }

    NBodySimProStatus status = force_function(view, gravity, force_context, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }

    for (size_t i = 0; i < particle_count; ++i) {
        velocities_x[i] += accelerations_x[i] * half_timestep;
        velocities_y[i] += accelerations_y[i] * half_timestep;
        velocities_z[i] += accelerations_z[i] * half_timestep;
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

static NBodySimProStatus advance_velocity_verlet(NBodySimProParticleSystemView* view,
                                            const NBodySimProGravity* gravity, double timestep,
                                            NBodySimProForceFunction force_function,
                                            void* force_context, NBodySimProError* error) {
    /*
     * Classic velocity Verlet updates are
     *   x' = x + v*dt + 0.5*a*dt^2
     *   a' = f(x')
     *   v' = v + 0.5*(a + a')*dt
     * which requires the old accelerations for the final kick. Applying the
     * velocity half-kick before the drift rewrites this as kick-drift-kick
     * with identical algebra for a constant timestep, so no scratch storage
     * is needed:
     *   v += 0.5*a*dt      (half kick with old a)
     *   x += v*dt          (v now carries the half-kick; equals x += v*dt + 0.5*a*dt^2)
     *   a' = f(x')
     *   v += 0.5*a'*dt
     */
    const size_t particle_count = view->particle_count;
    double* const positions_x = view->positions_x;
    double* const positions_y = view->positions_y;
    double* const positions_z = view->positions_z;
    double* const velocities_x = view->velocities_x;
    double* const velocities_y = view->velocities_y;
    double* const velocities_z = view->velocities_z;
    double* const accelerations_x = view->accelerations_x;
    double* const accelerations_y = view->accelerations_y;
    double* const accelerations_z = view->accelerations_z;

    const double half_timestep = 0.5 * timestep;

    for (size_t i = 0; i < particle_count; ++i) {
        velocities_x[i] += accelerations_x[i] * half_timestep;
        velocities_y[i] += accelerations_y[i] * half_timestep;
        velocities_z[i] += accelerations_z[i] * half_timestep;

        positions_x[i] += velocities_x[i] * timestep;
        positions_y[i] += velocities_y[i] * timestep;
        positions_z[i] += velocities_z[i] * timestep;
    }

    NBodySimProStatus status = force_function(view, gravity, force_context, error);
    if (status != N_BODY_SIM_PRO_STATUS_OK) {
        return status;
    }

    for (size_t i = 0; i < particle_count; ++i) {
        velocities_x[i] += accelerations_x[i] * half_timestep;
        velocities_y[i] += accelerations_y[i] * half_timestep;
        velocities_z[i] += accelerations_z[i] * half_timestep;
    }
    return N_BODY_SIM_PRO_STATUS_OK;
}

NBodySimProStatus n_body_sim_pro_integrator_advance(NBodySimProParticleSystemView* view,
                                       const NBodySimProGravity* gravity,
                                       NBodySimProIntegratorType integrator,
                                       double timestep,
                                       NBodySimProForceFunction force_function,
                                       void* force_context, NBodySimProError* error) {
    if (view == NULL || gravity == NULL || force_function == NULL) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "view, gravity, and force function must not be null");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }
    if (timestep <= 0.0) {
        n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                         "timestep must be positive");
        return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
    }

    switch (integrator) {
        case N_BODY_SIM_PRO_INTEGRATOR_EULER:
            return advance_euler(view, gravity, timestep, force_function, force_context,
                                 error);
        case N_BODY_SIM_PRO_INTEGRATOR_LEAPFROG:
            return advance_leapfrog(view, gravity, timestep, force_function, force_context,
                                    error);
        case N_BODY_SIM_PRO_INTEGRATOR_VELOCITY_VERLET:
            return advance_velocity_verlet(view, gravity, timestep, force_function,
                                           force_context, error);
    }
    n_body_sim_pro_error_set(error, N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT, __FILE__, __LINE__,
                     "unknown integrator type");
    return N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT;
}
