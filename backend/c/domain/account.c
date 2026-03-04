#include "include/account.h"

static bool account_build(
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

bool account_create(
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    Account *out,
    DomainError *err
) {
    AccountId dummy = { 0 };
    return account_build(false, dummy, user_id, account_type, currency,
                         balance_cents, iban, out, err);
}

bool account_rehydrate(
    AccountId id,
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    Account *out,
    DomainError *err
) {
    return account_build(true, id, user_id, account_type, currency,
                         balance_cents, iban, out, err);
}

/* Getters */

bool account_has_id(const Account *a) { return a && a->has_id; }
AccountId account_id(const Account *a) { return a->id; }
UserId account_user_id(const Account *a) { return a->user_id; }
AccountType account_type_get(const Account *a) { return a->account_type; }
Currency account_currency(const Account *a) { return a->currency; }
int64_t account_balance_cents(const Account *a) { return a->balance_cents; }
const IBAN *account_iban(const Account *a) { return &a->iban; }

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
    /* saturating_add Rust – C simple version */
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
