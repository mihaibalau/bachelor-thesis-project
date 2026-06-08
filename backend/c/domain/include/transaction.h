#ifndef C_TRANSACTION_H
#define C_TRANSACTION_H

#include "error.h"
#include "ids.h"
#include "transaction_type.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Opaque transaction type. */

typedef struct Transaction Transaction;

/* Constructors (heap-allocated) */

Transaction *transaction_create(
    AccountId from_account_id,
    AccountId to_account_id,
    TransactionType transaction_type,
    int64_t value_cents,
    const char *description,
    DomainError *err
);

Transaction *transaction_rehydrate(
    TransactionId id,
    AccountId from_account_id,
    AccountId to_account_id,
    TransactionType transaction_type,
    int64_t value_cents,
    time_t recorded_on,
    long recorded_on_micros,
    const char *description,
    DomainError *err
);

/* Getters */

bool           transaction_has_id(const Transaction *t);
TransactionId  transaction_id(const Transaction *t);
AccountId      transaction_from_account_id(const Transaction *t);
AccountId      transaction_to_account_id(const Transaction *t);
TransactionType transaction_type_get(const Transaction *t);
const char    *transaction_type_str(const Transaction *t);
int64_t        transaction_value_cents(const Transaction *t);
time_t         transaction_recorded_on(const Transaction *t);
/* Sub-second component of recorded_on, in microseconds (0..999999). */
long           transaction_recorded_on_micros(const Transaction *t);
const char    *transaction_description(const Transaction *t);

/* Persistence hook */

void transaction_set_id_after_insert(Transaction *t, TransactionId id);

/* Destructor */

void transaction_free(Transaction *t);

#endif /* C_TRANSACTION_H */
