#ifndef C_CURRENCY_H
#define C_CURRENCY_H

#include "error.h"
#include <stdbool.h>

typedef enum {
    CURRENCY_RON,
    CURRENCY_EUR,
    CURRENCY_USD
} Currency;

const char *currency_as_str(Currency c);
bool currency_from_str(const char *s, Currency *out, DomainError *err);

#endif