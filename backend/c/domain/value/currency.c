#include "../include/currency.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

const char *currency_as_str(Currency c) {
    switch (c) {
        case CURRENCY_RON: return "RON";
        case CURRENCY_EUR: return "EUR";
        case CURRENCY_USD: return "USD";
        default:           return "UNKNOWN";
    }
}

bool currency_from_str(const char *s, Currency *out, DomainError *err) {
    if (!s || !out) {
        if (err) *err = domain_error_validation("currency string is null");
        return false;
    }

    char upper[8];
    size_t len = strlen(s);
    if (len >= sizeof(upper)) len = sizeof(upper) - 1;
    for (size_t i = 0; i < len; ++i) upper[i] = (char)toupper((unsigned char)s[i]);
    upper[len] = '\0';

    if (!strcmp(upper, "RON")) {
        *out = CURRENCY_RON;
        if (err) *err = domain_error_ok();
        return true;
    }
    if (!strcmp(upper, "EUR")) {
        *out = CURRENCY_EUR;
        if (err) *err = domain_error_ok();
        return true;
    }
    if (!strcmp(upper, "USD")) {
        *out = CURRENCY_USD;
        if (err) *err = domain_error_ok();
        return true;
    }

    if (err) *err = domain_error_validation("Invalid currency type");
    return false;
}

const Currency *currency_all(size_t *out_count) {
    static const Currency ALL[CURRENCY_COUNT] = {
        CURRENCY_RON,
        CURRENCY_EUR,
        CURRENCY_USD
    };
    if (out_count) *out_count = CURRENCY_COUNT;
    return ALL;
}
