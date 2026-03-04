#ifndef C_TRANSACTION_TYPE_H
#define C_TRANSACTION_TYPE_H

#include "error.h"
#include <stdbool.h>

typedef enum {
    TRANSACTION_TYPE_DEPOSIT,
    TRANSACTION_TYPE_WITHDRAWAL,
    TRANSACTION_TYPE_SEND,
    TRANSACTION_TYPE_TRANSFER
} TransactionType;

const char *transaction_type_as_str(TransactionType t);
bool transaction_type_from_str(const char *s, TransactionType *out, DomainError *err);

#endif