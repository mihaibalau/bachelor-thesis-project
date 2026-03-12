#ifndef C_TRANSACTION_REPO_H
#define C_TRANSACTION_REPO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "db.h"
#include "repo_error.h"
#include "ids.h"
#include "transaction.h"  /* Transaction opaque type */

typedef struct TransactionRepo TransactionRepo;

TransactionRepo *transaction_repo_new(Db *db);
void             transaction_repo_free(TransactionRepo *repo);

bool transaction_repo_get_by_id(
    TransactionRepo *repo,
    TransactionId id,
    Transaction **out,
    RepoError *err
);

bool transaction_repo_insert(
    TransactionRepo *repo,
    const Transaction *tx,
    TransactionId *out_id,
    RepoError *err
);

bool transaction_repo_list_for_account(
    TransactionRepo *repo,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    RepoError *err
);

#endif //C_TRANSACTION_REPO_H