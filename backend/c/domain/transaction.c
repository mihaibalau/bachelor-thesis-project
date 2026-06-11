#include "include/transaction.h"

#include <assert.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Transaction {
    bool has_id;
    TransactionId id;

    AccountId from_account_id;
    AccountId to_account_id;

    TransactionType transaction_type;
    int64_t value_cents;

    time_t recorded_on;
    long   recorded_on_micros;   /* sub-second component, 0..999999 */
    char description[256];
};

static bool normalize_required_str(
    const char *s,
    const char *field,
    char *out,
    size_t out_size,
    DomainError *err
) {
    if (!s) {
        if (err) *err = domain_error_validation("field is null");
        return false;
    }

    const char *start = s;
    while (*start && isspace((unsigned char)*start)) ++start;

    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) --end;

    size_t len = (size_t)(end - start);
    if (len == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s must not be empty", field);
        if (err) *err = domain_error_validation(msg);
        return false;
    }

    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';

    if (err) *err = domain_error_ok();
    return true;
}

static bool transaction_init(
    bool has_id,
    TransactionId id,
    AccountId from_account_id,
    AccountId to_account_id,
    TransactionType transaction_type,
    int64_t value_cents,
    time_t recorded_on_opt,
    long recorded_on_micros_opt,
    bool has_recorded_on,
    const char *description,
    Transaction *out,
    DomainError *err
) {
    // Value >= 0, from != to, non-empty description.
    if (!out) {
        if (err) *err = domain_error_validation("Transaction is null");
        return false;
    }

    if (value_cents < 0) {
        if (err) *err = domain_error_validation("Value must be >= 0");
        return false;
    }

    if (from_account_id.value == to_account_id.value) {
        if (err) *err = domain_error_validation(
                "From Account ID must be different from To Account ID");
        return false;
    }

    Transaction tmp;
    tmp.has_id = has_id;
    tmp.id = id;
    tmp.from_account_id = from_account_id;
    tmp.to_account_id = to_account_id;
    tmp.transaction_type = transaction_type;
    tmp.value_cents = value_cents;
    if (has_recorded_on) {
        // Rehydrating an existing row: keep the stored full-precision timestamp.
        tmp.recorded_on        = recorded_on_opt;
        tmp.recorded_on_micros = recorded_on_micros_opt;
    } else {
        // New transaction: capture wall-clock time with microsecond precision
        // (parity with Rust's Utc::now()).
        struct timespec ts;
        if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
            tmp.recorded_on        = ts.tv_sec;
            tmp.recorded_on_micros = ts.tv_nsec / 1000;
        } else {
            tmp.recorded_on        = time(NULL);
            tmp.recorded_on_micros = 0;
        }
    }

    if (!normalize_required_str(description, "Description",
                                tmp.description, sizeof(tmp.description), err)) {
        return false;
    }

    *out = tmp;
    return true;
}

static Transaction *transaction_new_object(DomainError *err) {
    Transaction *t = (Transaction *)malloc(sizeof *t);
    if (!t) {
        if (err) *err = domain_error_validation("Transaction: out of memory");
        return NULL;
    }
    return t;
}

Transaction *transaction_create(
    AccountId from_account_id,
    AccountId to_account_id,
    TransactionType transaction_type,
    int64_t value_cents,
    const char *description,
    DomainError *err
) {
    TransactionId dummy = { 0 };
    Transaction *t = transaction_new_object(err);
    if (!t) {
        return NULL;
    }

    if (!transaction_init(false, dummy, from_account_id, to_account_id,
                          transaction_type, value_cents, 0, 0, false,
                          description, t, err)) {
        free(t);
        return NULL;
    }

    return t;
}

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
) {
    Transaction *t = transaction_new_object(err);
    if (!t) {
        return NULL;
    }

    if (!transaction_init(true, id, from_account_id, to_account_id,
                          transaction_type, value_cents, recorded_on,
                          recorded_on_micros, true,
                          description, t, err)) {
        free(t);
        return NULL;
    }

    return t;
}

bool transaction_has_id(const Transaction *t) {
    assert(t);
    return t->has_id;
}

TransactionId transaction_id(const Transaction *t) {
    assert(t);
    return t->id;
}

AccountId transaction_from_account_id(const Transaction *t) {
    assert(t);
    return t->from_account_id;
}

AccountId transaction_to_account_id(const Transaction *t) {
    assert(t);
    return t->to_account_id;
}

TransactionType transaction_type_get(const Transaction *t) {
    assert(t);
    return t->transaction_type;
}

const char *transaction_type_str(const Transaction *t) {
    assert(t);
    return transaction_type_as_str(t->transaction_type);
}

int64_t transaction_value_cents(const Transaction *t) {
    assert(t);
    return t->value_cents;
}

time_t transaction_recorded_on(const Transaction *t) {
    assert(t);
    return t->recorded_on;
}

long transaction_recorded_on_micros(const Transaction *t) {
    assert(t);
    return t->recorded_on_micros;
}

const char *transaction_description(const Transaction *t) {
    assert(t);
    return t->description;
}

void transaction_set_id_after_insert(Transaction *t, TransactionId id) {
    if (!t) return;
    t->has_id = true;
    t->id = id;
}

void transaction_free(Transaction *t) {
    free(t);
}
