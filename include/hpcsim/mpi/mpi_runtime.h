#ifndef HPCSIM_MPI_MPI_RUNTIME_H
#define HPCSIM_MPI_MPI_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MPI runtime wrapper.
 *
 * The distributed layer is compiled only when the engine is built with MPI
 * (HPCSIM_HAVE_MPI). Every entry point degrades gracefully to a single-rank
 * "no MPI" execution when MPI is absent, so the rest of the engine links and
 * runs unchanged.
 */

typedef struct HpcsimMpiRuntime {
    int available; /* this process is inside an MPI job */
    int rank;
    int comm_size;
} HpcsimMpiRuntime;

/* 1 when MPI was initialized for this process. */
int hpcsim_mpi_available(void);
int hpcsim_mpi_rank(void);
int hpcsim_mpi_comm_size(void);

/*
 * Initialize MPI (idempotent). When the process was launched by mpiexec this
 * joins MPI_COMM_WORLD; otherwise it reports available=0. Returns 0 on
 * success.
 */
int hpcsim_mpi_initialize(int* argc, char*** argv, HpcsimMpiRuntime* runtime);
void hpcsim_mpi_finalize(void);

int hpcsim_mpi_barrier(void);
void hpcsim_mpi_abort(int exit_code);

/* Seconds since a monotonic clock shared across ranks (best effort). */
double hpcsim_mpi_wall_time(void);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_MPI_MPI_RUNTIME_H */
