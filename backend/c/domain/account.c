#include "include/account.h"

#include <assert.h>
#include <stdlib.h>

/* Concrete representation, hidden from consumers. */
struct Account {
    bool has_id;
    AccountId id;

    UserId user_id;
    AccountType account_type;
    Currency currency;
    int64_t balance_cents;
    IBAN iban;
};

static bool account_init(
    bool has_id,
    AccountId id,
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    Account *out,
    DomainError *err
) {
    if (!out || !iban) {
        if (err) *err = domain_error_validation("Account: invalid arguments");
        return false;
    }

    if (balance_cents < 0) {
        if (err) *err = domain_error_validation("Balance must be >= 0");
        return false;
    }

    out->has_id = has_id;
    out->id = id;
    out->user_id = user_id;
    out->account_type = account_type;
    out->currency = currency;
    out->balance_cents = balance_cents;
    out->iban = *iban;

    if (err) *err = domain_error_ok();
    return true;
}

static Account *account_new_object(DomainError *err) {
    Account *a = (Account *)malloc(sizeof *a);
    if (!a) {
        if (err) *err = domain_error_validation("Account: out of memory");
        return NULL;
    }
    return a;
}

/* Public API */

Account *account_create(
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    DomainError *err
) {
    AccountId dummy = { 0 };
    Account *a = account_new_object(err);
    if (!a) {
        return NULL;
    }

    if (!account_init(false, dummy, user_id, account_type, currency,
                      balance_cents, iban, a, err)) {
        free(a);
        return NULL;
    }

    return a;
}

Account *account_rehydrate(
    AccountId id,
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    DomainError *err
) {
    Account *a = account_new_object(err);
    if (!a) {
        return NULL;
    }

    if (!account_init(true, id, user_id, account_type, currency,
                      balance_cents, iban, a, err)) {
        free(a);
        return NULL;
    }

    return a;
}

/* Getters */

bool account_has_id(const Account *a) {
    assert(a);
    return a->has_id;
}

AccountId account_id(const Account *a) {
    assert(a);
    return a->id;
}

UserId account_user_id(const Account *a) {
    assert(a);
    return a->user_id;
}

AccountType account_type_get(const Account *a) {
    assert(a);
    return a->account_type;
}

Currency account_currency(const Account *a) {
    assert(a);
    return a->currency;
}

int64_t account_balance_cents(const Account *a) {
    assert(a);
    return a->balance_cents;
}

const IBAN *account_iban(const Account *a) {
    assert(a);
    return &a->iban;
}

/* Business logic */

bool account_credit(Account *a, int64_t amount_cents, DomainError *err) {
    if (!a) {
        if (err) *err = domain_error_validation("Account is null");
        return false;
    }
    if (amount_cents <= 0) {
        if (err) *err = domain_error_validation("Credit amount must be > 0");
        return false;
    }
    if (amount_cents > 0 && a->balance_cents > INT64_MAX - amount_cents) {
        if (err) *err = domain_error_validation("Balance overflow");
        return false;
    }

    a->balance_cents += amount_cents;
    if (err) *err = domain_error_ok();
    return true;
}

bool account_debit(Account *a, int64_t amount_cents, DomainError *err) {
    if (!a) {
        if (err) *err = domain_error_validation("Account is null");
        return false;
    }
    if (amount_cents <= 0) {
        if (err) *err = domain_error_validation("Debit amount must be > 0");
        return false;
    }
    if (a->balance_cents < amount_cents) {
        if (err) *err = domain_error_validation("Insufficient funds");
        return false;
    }

    a->balance_cents -= amount_cents;
    if (err) *err = domain_error_ok();
    return true;
}

void account_set_id_after_insert(Account *a, AccountId id) {
    if (!a) return;
    a->has_id = true;
    a->id = id;
}

/* Destructor */

void account_free(Account *a) {
    free(a);
}
