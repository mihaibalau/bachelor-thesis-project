#ifndef HTTP_ERROR_H
#define HTTP_ERROR_H

#include "service/include/service_error.h"

typedef struct ApiErrorBody {
    const char *code;     // e.g. "not_found"
    const char *message;  // pointer inside the ServiceError
    int         status;   // HTTP status, ex. 404
} ApiErrorBody;

ApiErrorBody http_error_from_service_error(const ServiceError *err);

#endif