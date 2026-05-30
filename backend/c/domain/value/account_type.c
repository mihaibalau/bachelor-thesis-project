#include "../include/account_type.h"
#include <string.h>
#include <strings.h>

const char *account_type_as_str(AccountType t) {
    switch (t) {
        case ACCOUNT_TYPE_SAVINGS: return "Savings";
        case ACCOUNT_TYPE_CREDIT:  return "Credit";
        case ACCOUNT_TYPE_REGULAR: return "Regular";
        default:                   return "Unknown";
    }
}

bool account_type_from_str(const char *s, AccountType *out, DomainError *err) {
    if (!s || !out) {
        if (err) *err = domain_error_validation("account type string is null");
        return false;
    }

    if (!strcasecmp(s, "Savings")) {
        *out = ACCOUNT_TYPE_SAVINGS;
        if (err) *err = domain_error_ok();
        return true;
    }
    if (!strcasecmp(s, "Credit")) {
        *out = ACCOUNT_TYPE_CREDIT;
        if (err) *err = domain_error_ok();
        return true;
    }
    if (!strcasecmp(s, "Regular")) {
        *out = ACCOUNT_TYPE_REGULAR;
        if (err) *err = domain_error_ok();
        return true;
    }

    if (err) *err = domain_error_validation("Invalid account type");
    return false;
}

const AccountType *account_type_all(size_t *out_count) {
    static const AccountType ALL[ACCOUNT_TYPE_COUNT] = {
        ACCOUNT_TYPE_SAVINGS,
        ACCOUNT_TYPE_CREDIT,
        ACCOUNT_TYPE_REGULAR
    };
    if (out_count) *out_count = ACCOUNT_TYPE_COUNT;
    return ALL;
}
