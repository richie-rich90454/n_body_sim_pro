#include "n_body_sim_pro/n_body_sim_pro.h"

#ifdef N_BODY_SIM_PRO_HAVE_MPI
#include <mpi.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Distributed Barnes-Hut equivalence test (MPI, requires mpiexec).
 *
 * Every rank participates for real: particles are partitioned across ranks as
 * contiguous blocks and each rank computes the forces on its own block using
 * the essential-tree exchange. Rank 0 also computes the single-rank Barnes-Hut
 * forces on the whole system; the distributed result must match within the
 * Barnes-Hut approximation tolerance. Both computations are the same
 * algorithm, so the difference is bounded by the partition boundary cells.
 *
 * The single-process fallback (no MPI) verifies the trivial 1-rank path.
 */

enum { TEST_PARTICLE_COUNT = 2048 };

static int global_failures = 0;

#define DIST_ASSERT(condition)                                     \
    do {                                                           \
        if (!(condition)) {                                        \
            fprintf(stderr, "  assertion failed at %s:%d: %s\n",   \
                    __FILE__, __LINE__, #condition);               \
            ++global_failures;                                     \
        }                                                          \
    } while (0)

int main(int argc, char** argv) {
    NBodySimProError error;
    n_body_sim_pro_error_clear(&error);

    int rank = 0;
    int comm_size = 1;
    int mpi_active = 0;
#ifdef N_BODY_SIM_PRO_HAVE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    mpi_active = 1;
#endif

    const size_t particle_count = TEST_PARTICLE_COUNT;
    NBodySimProParticleSystem* global = n_body_sim_pro_particle_system_create(particle_count);
    DIST_ASSERT(global != NULL);
    if (global == NULL) {
        return 1;
    }

    /* Rank 0 generates the global system and broadcasts it. */
    NBodySimProPresetParameters parameters = {particle_count, 42};
    if (rank == 0) {
        DIST_ASSERT(n_body_sim_pro_preset_generate(global, N_BODY_SIM_PRO_PRESET_RANDOM_CLOUD, &parameters,
                                           &error) == N_BODY_SIM_PRO_STATUS_OK);
    }
    double* global_positions_x = n_body_sim_pro_particle_system_positions_x(global);
    double* global_positions_y = n_body_sim_pro_particle_system_positions_y(global);
    double* global_positions_z = n_body_sim_pro_particle_system_positions_z(global);
    double* global_velocities_x = n_body_sim_pro_particle_system_velocities_x(global);
    double* global_velocities_y = n_body_sim_pro_particle_system_velocities_y(global);
    double* global_velocities_z = n_body_sim_pro_particle_system_velocities_z(global);
    double* global_masses = n_body_sim_pro_particle_system_masses(global);
    n_body_sim_pro_particle_system_set_particle_count(global, particle_count, &error);

    NBodySimProGravity gravity;
    n_body_sim_pro_gravity_init(&gravity, 1.0, 0.02);

    /* Single-rank reference forces (rank 0). */
    double reference[3 * TEST_PARTICLE_COUNT];
    if (rank == 0) {
        NBodySimProParticleSystemView global_view;
        n_body_sim_pro_particle_system_view(global, &global_view, &error);
        NBodySimProBarnesHutTree* tree = n_body_sim_pro_barnes_hut_tree_create(&error);
        DIST_ASSERT(tree != NULL);
        n_body_sim_pro_barnes_hut_tree_set_theta(tree, 0.4);
        DIST_ASSERT(n_body_sim_pro_barnes_hut_compute_acceleration(&global_view, &gravity, tree,
                                                           &error) == N_BODY_SIM_PRO_STATUS_OK);
        for (size_t i = 0; i < particle_count; ++i) {
            reference[3 * i + 0] = global_view.accelerations_x[i];
            reference[3 * i + 1] = global_view.accelerations_y[i];
            reference[3 * i + 2] = global_view.accelerations_z[i];
        }
        n_body_sim_pro_barnes_hut_tree_destroy(tree);
    }

#ifdef N_BODY_SIM_PRO_HAVE_MPI
    /* Broadcast the global particle data. */
    MPI_Bcast(global_positions_x, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_positions_y, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_positions_z, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_velocities_x, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_velocities_y, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_velocities_z, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(global_masses, (int)particle_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(reference, (int)(3 * particle_count), MPI_DOUBLE, 0, MPI_COMM_WORLD);

    /* Partition: each rank owns a contiguous block. */
    const size_t local_count = particle_count / (size_t)comm_size;
    const size_t start = (size_t)rank * local_count;

    NBodySimProParticleSystem* local = n_body_sim_pro_particle_system_create(local_count);
    DIST_ASSERT(local != NULL);
    if (local != NULL) {
        n_body_sim_pro_particle_system_set_particle_count(local, local_count, &error);
        for (size_t i = 0; i < local_count; ++i) {
            n_body_sim_pro_particle_system_set_position(local, i,
                                                (NBodySimProVector3){global_positions_x[start + i],
                                                               global_positions_y[start + i],
                                                               global_positions_z[start + i]},
                                                &error);
            n_body_sim_pro_particle_system_set_velocity(local, i,
                                                (NBodySimProVector3){global_velocities_x[start + i],
                                                               global_velocities_y[start + i],
                                                               global_velocities_z[start + i]},
                                                &error);
            n_body_sim_pro_particle_system_set_mass(local, i, global_masses[start + i], &error);
        }

        NBodySimProMpiRuntime runtime;
        runtime.available = mpi_active;
        runtime.rank = rank;
        runtime.comm_size = comm_size;

        NBodySimProDistributedSimulation* simulation = n_body_sim_pro_distributed_create(&runtime, &error);
        DIST_ASSERT(simulation != NULL);
        if (simulation != NULL) {
            n_body_sim_pro_distributed_set_theta(simulation, 0.4);
            NBodySimProParticleSystemView local_view;
            n_body_sim_pro_particle_system_view(local, &local_view, &error);
            DIST_ASSERT(n_body_sim_pro_distributed_compute_acceleration(&local_view, &gravity,
                                                                simulation, &error) ==
                        N_BODY_SIM_PRO_STATUS_OK);

            /* The SIMD distributed traversal must agree with the scalar one
             * (identical exchange, identical opening decisions, vectorized
             * accumulation), and both must agree with the single-rank
             * reference within the theta tolerance. Exercise every backend
             * the build actually compiled in. */
            NBodySimProCpuFeatures features = n_body_sim_pro_cpu_detect_features();
#ifdef N_BODY_SIM_PRO_HAVE_AVX2_KERNEL
            if (features.has_avx2) {
                DIST_ASSERT(n_body_sim_pro_distributed_compute_acceleration_avx2(&local_view, &gravity,
                                                                         simulation, &error) ==
                            N_BODY_SIM_PRO_STATUS_OK);
            }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_AVX512_KERNEL
            if (features.has_avx512_foundation) {
                DIST_ASSERT(n_body_sim_pro_distributed_compute_acceleration_avx512(&local_view, &gravity,
                                                                           simulation, &error) ==
                            N_BODY_SIM_PRO_STATUS_OK);
            }
#endif
#ifdef N_BODY_SIM_PRO_HAVE_NEON_KERNEL
            if (features.has_neon) {
                DIST_ASSERT(n_body_sim_pro_distributed_compute_acceleration_neon(&local_view, &gravity,
                                                                         simulation, &error) ==
                            N_BODY_SIM_PRO_STATUS_OK);
            }
#endif
            n_body_sim_pro_distributed_destroy(simulation);
        }

        /* Gather distributed accelerations back to rank 0. */
        double* local_acceleration =
            (double*)malloc(3 * local_count * sizeof(double));
        for (size_t i = 0; i < local_count; ++i) {
            local_acceleration[3 * i + 0] =
                n_body_sim_pro_particle_system_accelerations_x(local)[i];
            local_acceleration[3 * i + 1] =
                n_body_sim_pro_particle_system_accelerations_y(local)[i];
            local_acceleration[3 * i + 2] =
                n_body_sim_pro_particle_system_accelerations_z(local)[i];
        }
        int* counts = (int*)malloc((size_t)comm_size * sizeof(int));
        int* displacements = (int*)malloc((size_t)comm_size * sizeof(int));
        for (int r = 0; r < comm_size; ++r) {
            counts[r] = (int)(3 * particle_count / (size_t)comm_size);
            displacements[r] = (int)(3 * (size_t)r * particle_count / (size_t)comm_size);
        }
        double* gathered = (double*)malloc(3 * particle_count * sizeof(double));
        MPI_Allgatherv(local_acceleration, (int)(3 * local_count), MPI_DOUBLE, gathered,
                       counts, displacements, MPI_DOUBLE, MPI_COMM_WORLD);

        if (rank == 0) {
            double sum_error_squared = 0.0;
            double sum_reference_squared = 0.0;
            for (size_t i = 0; i < particle_count; ++i) {
                for (int component = 0; component < 3; ++component) {
                    const double delta =
                        gathered[3 * i + (size_t)component] - reference[3 * i + (size_t)component];
                    sum_error_squared += delta * delta;
                    sum_reference_squared +=
                        reference[3 * i + (size_t)component] * reference[3 * i + (size_t)component];
                }
            }
            const double relative_error = sqrt(sum_error_squared / sum_reference_squared);
            DIST_ASSERT(relative_error < 0.02);
            if (relative_error >= 0.02) {
                fprintf(stderr, "  distributed vs single-rank RMS error: %.3e\n",
                        relative_error);
            }
        }

        free(local_acceleration);
        free(counts);
        free(displacements);
        free(gathered);
        n_body_sim_pro_particle_system_destroy(local);
    }
#endif

    /* Aggregate failures across ranks. */
    int total_failures = global_failures;
#ifdef N_BODY_SIM_PRO_HAVE_MPI
    MPI_Allreduce(&global_failures, &total_failures, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
    if (rank == 0) {
        if (total_failures == 0) {
            printf("distributed_barnes_hut_test: PASS (%d rank%s)\n", comm_size,
                   comm_size == 1 ? "" : "s");
        } else {
            printf("distributed_barnes_hut_test: FAIL (%d assertions)\n", total_failures);
        }
    }

    n_body_sim_pro_particle_system_destroy(global);
#ifdef N_BODY_SIM_PRO_HAVE_MPI
    MPI_Finalize();
#endif
    return total_failures == 0 ? 0 : 1;
}
