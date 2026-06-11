#include "../include/iban.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

const char *iban_as_cstr(const IBAN *iban) {
    return iban ? iban->value : "";
}

bool iban_try_create(const char *raw, IBAN *out, DomainError *err) {
    if (!raw || !out) {
        if (err) *err = domain_error_validation("IBAN must not be null");
        return false;
    }

    // Allow one extra char so over-length input is detected (not silently truncated).
    char buffer[IBAN_MAX_LEN + 2];
    size_t j = 0;
    for (size_t i = 0; raw[i] != '\0' && j <= IBAN_MAX_LEN; ++i) {
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

// IBAN check digits: treat BBAN+country as a big integer mod 97 (ISO 13616).
// Letters expand to two digits (A→10 … Z→35); we fold digit-by-digit to stay in uint64.
static uint64_t mod97_str(const char *s) {
    uint64_t acc = 0;
    for (; *s; ++s) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') {
            // Letters expand to two digits (A→10 … Z→35).
            int n = (c - 'A') + 10;
            acc = (acc * 10 + (n / 10)) % 97;
            acc = (acc * 10 + (n % 10)) % 97;
        } else {
            acc = (acc * 10 + (c - '0')) % 97;
        }
    }
    return acc;
}

bool iban_generate(IBAN *out, DomainError *err) {
    if (!out) {
        if (err) *err = domain_error_validation("IBAN output must not be null");
        return false;
    }

    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }

    const char *country   = "RO";
    const char *bank_code = "GNTL";
    const char *branch    = "0000";

    char account[13];
    for (int i = 0; i < 12; ++i)
        account[i] = (char)('0' + rand() % 10);
    account[12] = '\0';

    char bban[21];
    snprintf(bban, sizeof bban, "%s%s%s", bank_code, branch, account);

    char raw[32];
    // Rearrange to BBAN+country+"00" so mod97 yields the check digits.
    snprintf(raw, sizeof raw, "%s%s00", bban, country);

    int check_digits = (int)(98 - mod97_str(raw));

    char iban_str[IBAN_MAX_LEN + 1];
    snprintf(iban_str, sizeof iban_str, "%s%02d%s", country, check_digits, bban);

    strcpy(out->value, iban_str);
    if (err) *err = domain_error_ok();
    return true;
}
