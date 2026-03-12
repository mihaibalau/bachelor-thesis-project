#ifndef C_ERROR_H
#define C_ERROR_H

#include <stdbool.h>

#define DOMAIN_ERROR_MESSAGE_MAX 256

typedef enum {
    DOMAIN_ERROR_NONE = 0,
    DOMAIN_ERROR_VALIDATION
} DomainErrorCode;

typedef struct {
    DomainErrorCode code;
    char message[DOMAIN_ERROR_MESSAGE_MAX];
} DomainError;

/* Constructors */

DomainError domain_error_ok(void);
DomainError domain_error_validation(const char *msg);

/* Helpers */

bool domain_error_is_ok(const DomainError *err);

#endif /* C_ERROR_H */
