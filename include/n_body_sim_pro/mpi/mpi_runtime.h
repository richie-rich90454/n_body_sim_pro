#ifndef N_BODY_SIM_PRO_MPI_MPI_RUNTIME_H
#define N_BODY_SIM_PRO_MPI_MPI_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MPI runtime wrapper.
 *
 * The distributed layer is compiled only when the engine is built with MPI
 * (N_BODY_SIM_PRO_HAVE_MPI). Every entry point degrades gracefully to a single-rank
 * "no MPI" execution when MPI is absent, so the rest of the engine links and
 * runs unchanged.
 */

typedef struct NBodySimProMpiRuntime {
    int available; /* this process is inside an MPI job */
    int rank;
    int comm_size;
} NBodySimProMpiRuntime;

/* 1 when MPI was initialized for this process. */
int n_body_sim_pro_mpi_available(void);
int n_body_sim_pro_mpi_rank(void);
int n_body_sim_pro_mpi_comm_size(void);

/*
 * Initialize MPI (idempotent). When the process was launched by mpiexec this
 * joins MPI_COMM_WORLD; otherwise it reports available=0. Returns 0 on
 * success.
 */
int n_body_sim_pro_mpi_initialize(int* argc, char*** argv, NBodySimProMpiRuntime* runtime);
void n_body_sim_pro_mpi_finalize(void);

int n_body_sim_pro_mpi_barrier(void);
void n_body_sim_pro_mpi_abort(int exit_code);

/* Seconds since a monotonic clock shared across ranks (best effort). */
double n_body_sim_pro_mpi_wall_time(void);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_MPI_MPI_RUNTIME_H */
