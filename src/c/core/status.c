#include "hpcsim/core/status.h"

#include <string.h>

const char* hpcsim_status_string(HpcsimStatus status) {
    switch (status) {
        case HPCSIM_STATUS_OK:
            return "ok";
        case HPCSIM_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case HPCSIM_STATUS_OUT_OF_MEMORY:
            return "out of memory";
        case HPCSIM_STATUS_INVALID_STATE:
            return "invalid state";
        case HPCSIM_STATUS_NOT_IMPLEMENTED:
            return "not implemented";
        case HPCSIM_STATUS_UNSUPPORTED_PLATFORM:
            return "unsupported platform";
        case HPCSIM_STATUS_OVERFLOW:
            return "overflow";
        case HPCSIM_STATUS_IO_ERROR:
            return "i/o error";
        case HPCSIM_STATUS_INSUFFICIENT_MEMORY:
            return "insufficient memory";
    }
    return "unknown status";
}

void hpcsim_error_set(HpcsimError* error, HpcsimStatus status,
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

void hpcsim_error_clear(HpcsimError* error) {
    if (error == NULL) {
        return;
    }
    error->status = HPCSIM_STATUS_OK;
    error->source_file = NULL;
    error->source_line = 0;
    error->message[0] = '\0';
}

int hpcsim_error_failed(const HpcsimError* error) {
    return error != NULL && error->status != HPCSIM_STATUS_OK;
}
