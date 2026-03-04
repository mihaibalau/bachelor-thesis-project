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
    strncpy(err.message, msg, sizeof(err.message) - 1);
    err.message[sizeof(err.message) - 1] = '\0';
    return err;
}

bool domain_error_is_ok(const DomainError *err) {
    return err == NULL || err->code == DOMAIN_ERROR_NONE;
}
