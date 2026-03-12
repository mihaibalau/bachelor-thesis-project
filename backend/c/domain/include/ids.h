#ifndef C_IDS_H
#define C_IDS_H

#include <stdint.h>

/* Strongly-typed IDs to avoid mixing entities. */

typedef struct {
    int64_t value;
} UserId;

typedef struct {
    int64_t value;
} AccountId;

typedef struct {
    int64_t value;
} TransactionId;

/* Conversions */

static inline UserId user_id_from_i64(int64_t v) {
    UserId id = { v };
    return id;
}

static inline int64_t user_id_to_i64(UserId id) {
    return id.value;
}

static inline AccountId account_id_from_i64(int64_t v) {
    AccountId id = { v };
    return id;
}

static inline int64_t account_id_to_i64(AccountId id) {
    return id.value;
}

static inline TransactionId transaction_id_from_i64(int64_t v) {
    TransactionId id = { v };
    return id;
}

static inline int64_t transaction_id_to_i64(TransactionId id) {
    return id.value;
}

#endif /* C_IDS_H */
