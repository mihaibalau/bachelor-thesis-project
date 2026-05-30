#ifndef C_ACCOUNT_TYPE_H
#define C_ACCOUNT_TYPE_H

#include "error.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ACCOUNT_TYPE_SAVINGS,
    ACCOUNT_TYPE_CREDIT,
    ACCOUNT_TYPE_REGULAR
} AccountType;

#define ACCOUNT_TYPE_COUNT 3

const char *account_type_as_str(AccountType t);

/* FromStr<AccountType> RUST */
bool account_type_from_str(const char *s, AccountType *out, DomainError *err);

/* Mirrors Rust AccountType::all(): static array of every variant. */
const AccountType *account_type_all(size_t *out_count);

#endif