#ifndef C_CURRENCY_H
#define C_CURRENCY_H

#include "error.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    CURRENCY_RON,
    CURRENCY_EUR,
    CURRENCY_USD
} Currency;

#define CURRENCY_COUNT 3

const char *currency_as_str(Currency c);
bool currency_from_str(const char *s, Currency *out, DomainError *err);

/* Mirrors Rust Currency::all(): static array of every variant. */
const Currency *currency_all(size_t *out_count);

#endif