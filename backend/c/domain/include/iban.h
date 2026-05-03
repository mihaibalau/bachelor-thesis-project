#ifndef C_IBAN_H
#define C_IBAN_H

#include "error.h"
#include <stdbool.h>

#define IBAN_MAX_LEN 24

typedef struct {
    char value[IBAN_MAX_LEN + 1];
} IBAN;

const char *iban_as_cstr(const IBAN *iban);
bool iban_try_create(const char *raw, IBAN *out, DomainError *err);
bool iban_generate(IBAN *out, DomainError *err);

#endif