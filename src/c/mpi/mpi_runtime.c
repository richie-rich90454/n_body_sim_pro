#include "hpcsim/mpi/mpi_runtime.h"

#ifdef HPCSIM_HAVE_MPI
#include <mpi.h>
#endif

#include <time.h>

static int mpi_initialized = 0;
static int mpi_rank = 0;
static int mpi_size = 1;

int hpcsim_mpi_available(void) {
    return mpi_initialized;
}

int hpcsim_mpi_rank(void) {
    return mpi_rank;
}

int hpcsim_mpi_comm_size(void) {
    return mpi_size;
}

int hpcsim_mpi_initialize(int* argc, char*** argv, HpcsimMpiRuntime* runtime) {
#ifdef HPCSIM_HAVE_MPI
    if (!mpi_initialized) {
        int provided = 0;
        if (MPI_Init_thread(argc, argv, MPI_THREAD_FUNNELED, &provided) != MPI_SUCCESS) {
            if (runtime != NULL) {
                runtime->available = 0;
                runtime->rank = 0;
                runtime->comm_size = 1;
            }
            return 1;
        }
        mpi_initialized = 1;
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    }
#else
    (void)argc;
    (void)argv;
#endif
    if (runtime != NULL) {
        runtime->available = mpi_initialized;
        runtime->rank = mpi_rank;
        runtime->comm_size = mpi_size;
    }
    return mpi_initialized ? 0 : 1;
}

void hpcsim_mpi_finalize(void) {
#ifdef HPCSIM_HAVE_MPI
    if (mpi_initialized) {
        MPI_Finalize();
        mpi_initialized = 0;
    }
#else
    (void)0;
#endif
}

int hpcsim_mpi_barrier(void) {
#ifdef HPCSIM_HAVE_MPI
    if (mpi_initialized) {
        return MPI_Barrier(MPI_COMM_WORLD) == MPI_SUCCESS ? 0 : 1;
    }
#else
    (void)0;
#endif
    return 0;
}

void hpcsim_mpi_abort(int exit_code) {
#ifdef HPCSIM_HAVE_MPI
    if (mpi_initialized) {
        MPI_Abort(MPI_COMM_WORLD, exit_code);
    }
#else
    (void)exit_code;
#endif
}

double hpcsim_mpi_wall_time(void) {
#ifdef HPCSIM_HAVE_MPI
    if (mpi_initialized) {
        return MPI_Wtime();
    }
#else
    (void)0;
#endif
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1.0e-9;
}
