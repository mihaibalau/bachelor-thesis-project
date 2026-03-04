#ifndef C_ACCOUNT_H
#define C_ACCOUNT_H

#include "error.h"
#include "ids.h"
#include "account_type.h"
#include "currency.h"
#include "iban.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool has_id;
    AccountId id;

    UserId user_id;
    AccountType account_type;
    Currency currency;
    int64_t balance_cents;
    IBAN iban;
} Account;

/* RUST: create / rehydrate */

bool account_create(
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    Account *out,
    DomainError *err
);

bool account_rehydrate(
    AccountId id,
    UserId user_id,
    AccountType account_type,
    Currency currency,
    int64_t balance_cents,
    const IBAN *iban,
    Account *out,
    DomainError *err
);

/* Getters */
bool       account_has_id(const Account *a);
AccountId  account_id(const Account *a);
UserId     account_user_id(const Account *a);
AccountType account_type_get(const Account *a);
Currency   account_currency(const Account *a);
int64_t    account_balance_cents(const Account *a);
const IBAN *account_iban(const Account *a);

/* Business */
bool account_credit(Account *a, int64_t amount_cents, DomainError *err);
bool account_debit(Account *a, int64_t amount_cents, DomainError *err);

/* Internal */
void account_set_id_after_insert(Account *a, AccountId id);

#endif