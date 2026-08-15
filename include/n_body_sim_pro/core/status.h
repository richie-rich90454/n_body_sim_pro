#ifndef N_BODY_SIM_PRO_CORE_STATUS_H
#define N_BODY_SIM_PRO_CORE_STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status codes returned by every public C engine function.
 *
 * Functions report success or failure through their return value. Functions
 * that need to convey richer context additionally accept an optional
 * NBodySimProError pointer; pass NULL when context is not required.
 */

typedef enum NBodySimProStatus {
    N_BODY_SIM_PRO_STATUS_OK = 0,
    N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT,
    N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY,
    N_BODY_SIM_PRO_STATUS_INVALID_STATE,
    N_BODY_SIM_PRO_STATUS_NOT_IMPLEMENTED,
    N_BODY_SIM_PRO_STATUS_UNSUPPORTED_PLATFORM,
    N_BODY_SIM_PRO_STATUS_OVERFLOW,
    N_BODY_SIM_PRO_STATUS_IO_ERROR,
    N_BODY_SIM_PRO_STATUS_INSUFFICIENT_MEMORY
} NBodySimProStatus;

typedef struct NBodySimProError {
    NBodySimProStatus status;
    const char* source_file;
    int source_line;
    char message[256];
} NBodySimProError;

/* Human-readable name for a status code. Never returns NULL. */
const char* n_body_sim_pro_status_string(NBodySimProStatus status);

/* Fill *error with the given failure context. NULL-safe. */
void n_body_sim_pro_error_set(NBodySimProError* error, NBodySimProStatus status,
                      const char* source_file, int source_line,
                      const char* message);

/* Reset *error to the OK state. NULL-safe. */
void n_body_sim_pro_error_clear(NBodySimProError* error);

/* Returns 1 when the error carries a failure status, 0 otherwise. */
int n_body_sim_pro_error_failed(const NBodySimProError* error);

#ifdef __cplusplus
}
#endif

#endif /* N_BODY_SIM_PRO_CORE_STATUS_H */
