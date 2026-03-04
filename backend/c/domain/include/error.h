#ifndef C_ERROR_H
#define C_ERROR_H

#include <stdbool.h>

typedef enum {
    DOMAIN_ERROR_NONE = 0,
    DOMAIN_ERROR_VALIDATION
} DomainErrorCode;

typedef struct {
    DomainErrorCode code;
    char message[256];
} DomainError;

DomainError domain_error_ok(void);
DomainError domain_error_validation(const char *msg);

bool domain_error_is_ok(const DomainError *err);

#endif