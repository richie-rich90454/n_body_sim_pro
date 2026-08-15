#include "n_body_sim_pro/core/status.h"

#include <string.h>

const char* n_body_sim_pro_status_string(NBodySimProStatus status) {
    switch (status) {
        case N_BODY_SIM_PRO_STATUS_OK:
            return "ok";
        case N_BODY_SIM_PRO_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case N_BODY_SIM_PRO_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        case N_BODY_SIM_PRO_STATUS_INVALID_STATE:
            return "invalid state";
        case N_BODY_SIM_PRO_STATUS_NOT_IMPLEMENTED:
            return "not implemented";
        case N_BODY_SIM_PRO_STATUS_UNSUPPORTED_PLATFORM:
            return "unsupported platform";
        case N_BODY_SIM_PRO_STATUS_OVERFLOW:
            return "overflow";
        case N_BODY_SIM_PRO_STATUS_IO_ERROR:
            return "i/o error";
        case N_BODY_SIM_PRO_STATUS_INSUFFICIENT_MEMORY:
            return "insufficient memory";
    }
    return "unknown status";
}

void n_body_sim_pro_error_set(NBodySimProError* error, NBodySimProStatus status,
                      const char* source_file, int source_line,
                      const char* message) {
    if (error == NULL) {
        return;
    }
    error->status = status;
    error->source_file = source_file;
    error->source_line = source_line;
    if (message != NULL) {
        size_t length = strlen(message);
        size_t copy = length < sizeof(error->message) ? length : sizeof(error->message) - 1;
        memcpy(error->message, message, copy);
        error->message[copy] = '\0';
    } else {
        error->message[0] = '\0';
    }
}

void n_body_sim_pro_error_clear(NBodySimProError* error) {
    if (error == NULL) {
        return;
    }
    error->status = N_BODY_SIM_PRO_STATUS_OK;
    error->source_file = NULL;
    error->source_line = 0;
    error->message[0] = '\0';
}

int n_body_sim_pro_error_failed(const NBodySimProError* error) {
    return error != NULL && error->status != N_BODY_SIM_PRO_STATUS_OK;
}
