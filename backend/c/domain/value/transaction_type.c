#include "../include/transaction_type.h"
#include <strings.h>
#include <string.h>

const char *transaction_type_as_str(TransactionType t) {
    switch (t) {
        case TRANSACTION_TYPE_DEPOSIT:    return "Deposit";
        case TRANSACTION_TYPE_WITHDRAWAL: return "Withdrawal";
        case TRANSACTION_TYPE_SEND:       return "Send";
        case TRANSACTION_TYPE_TRANSFER:   return "Transfer";
        case TRANSACTION_TYPE_PAYMENT:    return "Payment";
        default:                          return "Unknown";
    }
}

bool transaction_type_from_str(const char *s, TransactionType *out, DomainError *err) {
    if (!s || !out) {
        if (err) *err = domain_error_validation("transaction type string is null");
        return false;
    }

    if (!strcasecmp(s, "Deposit")) {
        *out = TRANSACTION_TYPE_DEPOSIT;
    } else if (!strcasecmp(s, "Withdrawal")) {
        *out = TRANSACTION_TYPE_WITHDRAWAL;
    } else if (!strcasecmp(s, "Send")) {
        *out = TRANSACTION_TYPE_SEND;
    } else if (!strcasecmp(s, "Transfer")) {
        *out = TRANSACTION_TYPE_TRANSFER;
    } else if (!strcasecmp(s, "Payment")) {
        *out = TRANSACTION_TYPE_PAYMENT;
    } else {
        if (err) *err = domain_error_validation("Invalid transaction type");
        return false;
    }

    if (err) *err = domain_error_ok();
    return true;
}
