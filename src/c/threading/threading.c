#include "n_body_sim_pro/threading/threading.h"

#ifdef _OPENMP
#include <omp.h>
#endif

int n_body_sim_pro_threading_available_thread_count(void) {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

void n_body_sim_pro_threading_set_thread_count(int thread_count) {
#ifdef _OPENMP
    if (thread_count <= 0) {
        omp_set_num_threads(omp_get_max_threads());
    } else {
        omp_set_num_threads(thread_count);
    }
#else
    (void)thread_count;
#endif
}

int n_body_sim_pro_threading_thread_count(void) {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

int n_body_sim_pro_threading_active_thread_count(void) {
#ifdef _OPENMP
    return omp_get_num_threads();
#else
    return 1;
#endif
}

int n_body_sim_pro_threading_openmp_available(void) {
#ifdef _OPENMP
    return 1;
#else
    return 0;
#endif
}
