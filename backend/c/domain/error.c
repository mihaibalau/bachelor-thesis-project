#include "include/error.h"

#include <string.h>

DomainError domain_error_ok(void) {
    DomainError err;
    err.code = DOMAIN_ERROR_NONE;
    err.message[0] = '\0';
    return err;
}

DomainError domain_error_validation(const char *msg) {
    DomainError err;
    err.code = DOMAIN_ERROR_VALIDATION;

    if (!msg) {
        err.message[0] = '\0';
        return err;
    }

    strncpy(err.message, msg, DOMAIN_ERROR_MESSAGE_MAX - 1);
    err.message[DOMAIN_ERROR_MESSAGE_MAX - 1] = '\0';
    return err;
}

bool domain_error_is_ok(const DomainError *err) {
    return err == NULL || err->code == DOMAIN_ERROR_NONE;
}
