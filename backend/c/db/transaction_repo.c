#include "include/transaction_repo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../util/include/util_str.h"
#include "../domain/include/transaction_type.h"
#include "../domain/include/error.h"

struct TransactionRepo {
    Db *db; /* non-owning */
};

TransactionRepo *transaction_repo_new(Db *db) {
    if (!db) return NULL;
    TransactionRepo *r = (TransactionRepo *)malloc(sizeof *r);
    if (!r) return NULL;
    r->db = db;
    return r;
}

void transaction_repo_free(TransactionRepo *repo) {
    free(repo);
}

static int cmd_rows_affected(PGresult *res) {
    const char *s = PQcmdTuples(res);
    if (!s || *s == '\0') return 0;
    return atoi(s);
}

static bool transaction_repo_row_to_domain(
    PGresult *res,
    int row,
    Transaction **out,
    RepoError *err
) {
    int col = 0;

    const char *id_s          = PQgetvalue(res, row, col++);
    const char *from_id_s     = PQgetvalue(res, row, col++);
    const char *to_id_s       = PQgetvalue(res, row, col++);
    const char *type_s        = PQgetvalue(res, row, col++);
    const char *value_s       = PQgetvalue(res, row, col++);
    const char *epoch_s       = PQgetvalue(res, row, col++);
    const char *desc_s        = PQgetvalue(res, row, col++);

    int64_t id_v, from_v, to_v, value_v, epoch_v;
    if (!util_str_to_i64(id_s, &id_v) ||
        !util_str_to_i64(from_id_s, &from_v) ||
        !util_str_to_i64(to_id_s, &to_v) ||
        !util_str_to_i64(value_s, &value_v) ||
        !util_str_to_i64(epoch_s, &epoch_v)) {
        if (err) *err = repo_error_db("TransactionRepo: invalid integer in result");
        return false;
    }

    TransactionId tx_id          = transaction_id_from_i64(id_v);
    AccountId     from_account   = account_id_from_i64(from_v);
    AccountId     to_account     = account_id_from_i64(to_v);
    time_t        recorded_on    = (time_t)epoch_v;

    TransactionType tx_type;
    DomainError derr;
    if (!transaction_type_from_str(type_s, &tx_type, &derr)) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    Transaction *tx = transaction_rehydrate(
        tx_id,
        from_account,
        to_account,
        tx_type,
        value_v,
        recorded_on,
        desc_s,
        &derr
    );
    if (!tx) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    if (err) *err = repo_error_ok();
    *out = tx;
    return true;
}

/* Public API */

bool transaction_repo_get_by_id(
    TransactionRepo *repo,
    TransactionId id,
    Transaction **out,
    RepoError *err
) {
    if (!repo || !out) {
        if (err) *err = repo_error_db("TransactionRepo::get_by_id: invalid arguments");
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)transaction_id_to_i64(id));
    const char *params[1] = { id_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT id, from_account_id, to_account_id, "
        "       transaction_type, value_cents, "
        "       EXTRACT(EPOCH FROM recorded_on)::BIGINT AS recorded_on_epoch, "
        "       description "
        "FROM transactions WHERE id = $1",
        1,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    if (rows == 0) {
        PQclear(res);
        if (err) *err = repo_error_not_found("transaction");
        return false;
    }

    bool ok = transaction_repo_row_to_domain(res, 0, out, err);
    PQclear(res);
    return ok;
}

bool transaction_repo_insert(
    TransactionRepo *repo,
    const Transaction *tx,
    TransactionId *out_id,
    RepoError *err
) {
    if (!repo || !tx || !out_id) {
        if (err) *err = repo_error_db("TransactionRepo::insert: invalid arguments");
        return false;
    }

    if (transaction_has_id(tx)) {
        DomainError derr = domain_error_validation(
            "Cannot insert a Transaction that already has an id");
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    char from_buf[32];
    snprintf(from_buf, sizeof from_buf, "%lld",
             (long long)account_id_to_i64(transaction_from_account_id(tx)));

    char to_buf[32];
    snprintf(to_buf, sizeof to_buf, "%lld",
             (long long)account_id_to_i64(transaction_to_account_id(tx)));

    const char *type_s = transaction_type_str(tx);

    char value_buf[32];
    snprintf(value_buf, sizeof value_buf, "%lld",
             (long long)transaction_value_cents(tx));

    time_t recorded_on = transaction_recorded_on(tx);
    char epoch_buf[32];
    snprintf(epoch_buf, sizeof epoch_buf, "%lld", (long long)recorded_on);

    const char *desc_s = transaction_description(tx);

    const char *params[6] = {
        from_buf,
        to_buf,
        type_s,
        value_buf,
        epoch_buf,
        desc_s
    };

    PGresult *res = db_exec_params(
        repo->db,
        "INSERT INTO transactions ("
        "    from_account_id, to_account_id, transaction_type, "
        "    value_cents, recorded_on, description"
        ") VALUES ("
        "    $1, $2, $3, $4, TO_TIMESTAMP($5), $6"
        ") RETURNING id",
        6,
        params,
        err
    );
    if (!res) {
        return false;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        if (err) *err = repo_error_db("TransactionRepo::insert: expected 1 row");
        return false;
    }

    const char *id_s = PQgetvalue(res, 0, 0);
    int64_t id_v;
    if (!util_str_to_i64(id_s, &id_v)) {
        PQclear(res);
        if (err) *err = repo_error_db("TransactionRepo::insert: invalid id");
        return false;
    }

    PQclear(res);
    *out_id = transaction_id_from_i64(id_v);
    if (err) *err = repo_error_ok();
    return true;
}

bool transaction_repo_list_for_account(
    TransactionRepo *repo,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    RepoError *err
) {
    if (!repo || !out_txs || !out_count) {
        if (err) *err = repo_error_db("TransactionRepo::list_for_account: invalid arguments");
        return false;
    }

    char acc_buf[32];
    snprintf(acc_buf, sizeof acc_buf, "%lld",
             (long long)account_id_to_i64(account_id));

    if (limit <= 0)  limit  = 50;
    if (offset < 0)  offset = 0;

    char limit_buf[32];
    snprintf(limit_buf, sizeof limit_buf, "%lld", (long long)limit);

    char offset_buf[32];
    snprintf(offset_buf, sizeof offset_buf, "%lld", (long long)offset);

    const char *params[3] = { acc_buf, limit_buf, offset_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT id, from_account_id, to_account_id, "
        "       transaction_type, value_cents, "
        "       EXTRACT(EPOCH FROM recorded_on)::BIGINT AS recorded_on_epoch, "
        "       description "
        "FROM transactions "
        "WHERE from_account_id = $1 OR to_account_id = $1 "
        "ORDER BY recorded_on DESC, id DESC "
        "LIMIT $2 OFFSET $3",
        3,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    if (rows < 0) rows = 0;

    Transaction **arr = NULL;
    if (rows > 0) {
        arr = (Transaction **)calloc((size_t)rows, sizeof(Transaction *));
        if (!arr) {
            PQclear(res);
            if (err) *err = repo_error_db("TransactionRepo::list_for_account: out of memory");
            return false;
        }
    }

    bool ok = true;
    for (int i = 0; i < rows; ++i) {
        Transaction *tx = NULL;
        if (!transaction_repo_row_to_domain(res, i, &tx, err)) {
            ok = false;
            for (int j = 0; j < i; ++j) {
                transaction_free(arr[j]);
            }
            free(arr);
            break;
        }
        arr[i] = tx;
    }

    PQclear(res);

    if (!ok) return false;

    *out_txs = arr;
    *out_count = (size_t)rows;
    if (err) *err = repo_error_ok();
    return true;
}
