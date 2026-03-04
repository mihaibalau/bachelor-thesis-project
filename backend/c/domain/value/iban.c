#include "../include/iban.h"

#include <ctype.h>
#include <string.h>

const char *iban_as_cstr(const IBAN *iban) {
    return iban ? iban->value : "";
}

bool iban_try_create(const char *raw, IBAN *out, DomainError *err) {
    if (!raw || !out) {
        if (err) *err = domain_error_validation("IBAN must not be null");
        return false;
    }

    char buffer[IBAN_MAX_LEN + 1];
    size_t j = 0;
    for (size_t i = 0; raw[i] != '\0' && j < IBAN_MAX_LEN; ++i) {
        char c = raw[i];
        if (!isspace((unsigned char)c)) {
            buffer[j++] = (char)toupper((unsigned char)c);
        }
    }
    buffer[j] = '\0';

    if (j == 0) {
        if (err) *err = domain_error_validation("IBAN must not be empty");
        return false;
    }
    if (j != 24) {
        if (err) *err = domain_error_validation("Invalid IBAN length");
        return false;
    }
    if (buffer[0] != 'R' || buffer[1] != 'O') {
        if (err) *err = domain_error_validation("Invalid country code");
        return false;
    }

    strcpy(out->value, buffer);
    if (err) *err = domain_error_ok();
    return true;
}
