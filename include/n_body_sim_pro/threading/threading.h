#ifndef N_BODY_SIM_PRO_THREADING_THREADING_H
#define N_BODY_SIM_PRO_THREADING_THREADING_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OpenMP thread configuration.
 *
 * The engine uses OpenMP for shared-memory parallelism. This module is the
 * single place that configures the active thread count. When the engine was
 * built without OpenMP every function degrades to a single-threaded default.
 */

/* Maximum number of threads the runtime would use with no limit. */
int n_body_sim_pro_threading_available_thread_count(void);

/*
 * Set the active thread count used by parallel regions.
 * `thread_count` <= 0 restores the default (all available threads).
 */
void n_body_sim_pro_threading_set_thread_count(int thread_count);

/* The currently configured thread count. */
int n_body_sim_pro_threading_thread_count(void);

/*
 * Number of threads executing the enclosing parallel region, or 1 when
 * called outside a parallel region.
 */
int n_body_sim_pro_threading_active_thread_count(void);

/* 1 when the engine was built with OpenMP support, 0 otherwise. */
int n_body_sim_pro_threading_openmp_available(void);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_THREADING_THREADING_H */
