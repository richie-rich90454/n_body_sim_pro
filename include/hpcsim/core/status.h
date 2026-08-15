#ifndef HPCSIM_CORE_STATUS_H
#define HPCSIM_CORE_STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status codes returned by every public C engine function.
 *
 * Functions report success or failure through their return value. Functions
 * that need to convey richer context additionally accept an optional
 * HpcsimError pointer; pass NULL when context is not required.
 */

typedef enum HpcsimStatus {
    HPCSIM_STATUS_OK = 0,
    HPCSIM_STATUS_INVALID_ARGUMENT,
    HPCSIM_STATUS_OUT_OF_MEMORY,
    HPCSIM_STATUS_INVALID_STATE,
    HPCSIM_STATUS_NOT_IMPLEMENTED,
    HPCSIM_STATUS_UNSUPPORTED_PLATFORM,
    HPCSIM_STATUS_OVERFLOW,
    HPCSIM_STATUS_IO_ERROR,
    HPCSIM_STATUS_INSUFFICIENT_MEMORY
} HpcsimStatus;

typedef struct HpcsimError {
    HpcsimStatus status;
    const char* source_file;
    int source_line;
    char message[256];
} HpcsimError;

/* Human-readable name for a status code. Never returns NULL. */
const char* hpcsim_status_string(HpcsimStatus status);

/* Fill *error with the given failure context. NULL-safe. */
void hpcsim_error_set(HpcsimError* error, HpcsimStatus status,
                      const char* source_file, int source_line,
                      const char* message);

/* Reset *error to the OK state. NULL-safe. */
void hpcsim_error_clear(HpcsimError* error);

/* Returns 1 when the error carries a failure status, 0 otherwise. */
int hpcsim_error_failed(const HpcsimError* error);

#ifdef __cplusplus
}
#endif

#endif /* HPCSIM_CORE_STATUS_H */
