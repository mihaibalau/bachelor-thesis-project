#include "include/account_repo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../util/include/util_str.h"
#include "../domain/include/account_type.h"
#include "../domain/include/currency.h"
#include "../domain/include/iban.h"
#include "../domain/include/error.h"

struct AccountRepo {
    Db *db;
};

AccountRepo *account_repo_new(Db *db) {
    if (!db) return NULL;
    AccountRepo *r = (AccountRepo *)malloc(sizeof *r);
    if (!r) return NULL;
    r->db = db;
    return r;
}

void account_repo_free(AccountRepo *repo) {
    free(repo);
}

// Map PG row -> domain Account.
static bool account_repo_row_to_domain(
    PGresult *res,
    int row,
    Account **out,
    RepoError *err
) {
    int col = 0;

    const char *id_s          = PQgetvalue(res, row, col++);
    const char *user_id_s     = PQgetvalue(res, row, col++);
    const char *account_type_s= PQgetvalue(res, row, col++);
    const char *currency_s    = PQgetvalue(res, row, col++);
    const char *balance_s     = PQgetvalue(res, row, col++);
    const char *iban_s        = PQgetvalue(res, row, col++);

    int64_t id_v, user_id_v, balance_v;
    if (!util_str_to_i64(id_s, &id_v) ||
        !util_str_to_i64(user_id_s, &user_id_v) ||
        !util_str_to_i64(balance_s, &balance_v)) {
        if (err) *err = repo_error_db("AccountRepo: invalid integer in result");
        return false;
    }

    AccountId   id       = account_id_from_i64(id_v);
    UserId      user_id  = user_id_from_i64(user_id_v);

    AccountType account_type;
    DomainError derr;
    if (!account_type_from_str(account_type_s, &account_type, &derr)) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    Currency currency;
    if (!currency_from_str(currency_s, &currency, &derr)) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    IBAN iban;
    if (!iban_try_create(iban_s, &iban, &derr)) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    Account *acc = account_rehydrate(
        id,
        user_id,
        account_type,
        currency,
        balance_v,
        &iban,
        &derr
    );
    if (!acc) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    if (err) *err = repo_error_ok();
    *out = acc;
    return true;
}

bool account_repo_get_by_id(AccountRepo *repo, AccountId id, Account **out, RepoError *err) {
    // 1. Query by id; map row to domain or NotFound.
    if (!repo || !out) {
        if (err) *err = repo_error_db("AccountRepo::get_by_id: invalid arguments");
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)account_id_to_i64(id));
    const char *params[1] = { id_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT id, user_id, account_type, currency, balance_cents, iban "
        "FROM accounts WHERE id = $1",
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
        if (err) *err = repo_error_not_found("account");
        return false;
    }

    bool ok = account_repo_row_to_domain(res, 0, out, err);
    PQclear(res);
    return ok;
}

bool account_repo_get_by_iban(AccountRepo *repo, const char *iban_str, Account **out, RepoError *err) {
    if (!repo || !out || !iban_str) {
        if (err) *err = repo_error_db("AccountRepo::get_by_iban: invalid arguments");
        return false;
    }

    const char *params[1] = { iban_str };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT id, user_id, account_type, currency, balance_cents, iban "
        "FROM accounts WHERE iban = $1",
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
        if (err) *err = repo_error_not_found("account");
        return false;
    }

    bool ok = account_repo_row_to_domain(res, 0, out, err);
    PQclear(res);
    return ok;
}

bool account_repo_list_for_user(
    AccountRepo *repo,
    UserId user_id,
    Account ***out_accounts,
    size_t *out_count,
    RepoError *err
) {
    if (!repo || !out_accounts || !out_count) {
        if (err) *err = repo_error_db("AccountRepo::list_for_user: invalid arguments");
        return false;
    }

    char user_buf[32];
    snprintf(user_buf, sizeof user_buf, "%lld", (long long)user_id_to_i64(user_id));
    const char *params[1] = { user_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT id, user_id, account_type, currency, balance_cents, iban "
        "FROM accounts WHERE user_id = $1 ORDER BY id",
        1,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    if (rows < 0) rows = 0;

    Account **arr = NULL;
    if (rows > 0) {
        arr = (Account **)calloc((size_t)rows, sizeof(Account *));
        if (!arr) {
            PQclear(res);
            if (err) *err = repo_error_db("AccountRepo::list_for_user: out of memory");
            return false;
        }
    }

    bool ok = true;
    for (int i = 0; i < rows; ++i) {
        Account *a = NULL;
        if (!account_repo_row_to_domain(res, i, &a, err)) {
            ok = false;
            for (int j = 0; j < i; ++j) {
                account_free(arr[j]);
            }
            free(arr);
            break;
        }
        arr[i] = a;
    }

    PQclear(res);

    if (!ok) return false;

    *out_accounts = arr;
    *out_count = (size_t)rows;
    if (err) *err = repo_error_ok();
    return true;
}

bool account_repo_insert(AccountRepo *repo, const Account *account, AccountId *out_id, RepoError *err) {
    // 1. INSERT row; return generated id.
    if (!repo || !account || !out_id) {
        if (err) *err = repo_error_db("AccountRepo::insert: invalid arguments");
        return false;
    }

    if (account_has_id(account)) {
        if (err) *err = repo_error_from_domain(
                &(DomainError){ DOMAIN_ERROR_VALIDATION, "Cannot insert an Account that already has an id" });
        return false;
    }

    char user_buf[32];
    snprintf(user_buf, sizeof user_buf, "%lld", (long long)user_id_to_i64(account_user_id(account)));

    const char *account_type_s = account_type_as_str(account_type_get(account));
    const char *currency_s     = currency_as_str(account_currency(account));

    char balance_buf[32];
    snprintf(balance_buf, sizeof balance_buf, "%lld", (long long)account_balance_cents(account));

    const char *iban_s = iban_as_cstr(account_iban(account));

    const char *params[5] = {
        user_buf,
        account_type_s,
        currency_s,
        balance_buf,
        iban_s
    };

    PGresult *res = db_exec_params(
        repo->db,
        "INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban) "
        "VALUES ($1, $2, $3, $4, $5) RETURNING id",
        5,
        params,
        err
    );
    if (!res) {
        return false;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        if (err) *err = repo_error_db("AccountRepo::insert: expected 1 row");
        return false;
    }

    const char *id_s = PQgetvalue(res, 0, 0);
    int64_t id_v;
    if (!util_str_to_i64(id_s, &id_v)) {
        PQclear(res);
        if (err) *err = repo_error_db("AccountRepo::insert: invalid id");
        return false;
    }

    PQclear(res);

    *out_id = account_id_from_i64(id_v);
    if (err) *err = repo_error_ok();
    return true;
}

static int cmd_rows_affected(PGresult *res) {
    const char *s = PQcmdTuples(res);
    if (!s || *s == '\0') return 0;
    return atoi(s);
}

bool account_repo_update(AccountRepo *repo, const Account *account, RepoError *err) {
    if (!repo || !account) {
        if (err) *err = repo_error_db("AccountRepo::update: invalid arguments");
        return false;
    }

    if (!account_has_id(account)) {
        if (err) *err = repo_error_from_domain(
                &(DomainError){ DOMAIN_ERROR_VALIDATION, "Cannot update an Account without an id" });
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)account_id_to_i64(account_id(account)));

    char user_buf[32];
    snprintf(user_buf, sizeof user_buf, "%lld", (long long)user_id_to_i64(account_user_id(account)));

    const char *account_type_s = account_type_as_str(account_type_get(account));
    const char *currency_s     = currency_as_str(account_currency(account));

    char balance_buf[32];
    snprintf(balance_buf, sizeof balance_buf, "%lld", (long long)account_balance_cents(account));

    const char *iban_s = iban_as_cstr(account_iban(account));

    const char *params[6] = {
        user_buf,
        account_type_s,
        currency_s,
        balance_buf,
        iban_s,
        id_buf
    };

    PGresult *res = db_exec_params(
        repo->db,
        "UPDATE accounts SET user_id = $1, account_type = $2, currency = $3, "
        "balance_cents = $4, iban = $5 WHERE id = $6",
        6,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = cmd_rows_affected(res);
    PQclear(res);

    if (rows == 0) {
        if (err) *err = repo_error_not_found("account");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

bool account_repo_delete(AccountRepo *repo, AccountId id, RepoError *err) {
    if (!repo) {
        if (err) *err = repo_error_db("AccountRepo::delete: invalid arguments");
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)account_id_to_i64(id));
    const char *params[1] = { id_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "DELETE FROM accounts WHERE id = $1",
        1,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = cmd_rows_affected(res);
    PQclear(res);

    if (rows == 0) {
        if (err) *err = repo_error_not_found("account");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

bool account_repo_exists_by_iban(AccountRepo *repo, const char *iban_str, bool *out_exists, RepoError *err) {
    if (!repo || !iban_str || !out_exists) {
        if (err) *err = repo_error_db("AccountRepo::exists_by_iban: invalid arguments");
        return false;
    }

    const char *params[1] = { iban_str };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT 1 FROM accounts WHERE iban = $1 LIMIT 1",
        1,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    PQclear(res);

    *out_exists = (rows > 0);
    if (err) *err = repo_error_ok();
    return true;
}

bool account_repo_exists_by_account_type(
    AccountRepo *repo,
    UserId user_id,
    AccountType account_type,
    bool *out_exists,
    RepoError *err
) {
    if (!repo || !out_exists) {
        if (err) *err = repo_error_db("AccountRepo::exists_by_account_type: invalid arguments");
        return false;
    }

    char user_buf[32];
    snprintf(user_buf, sizeof user_buf, "%lld", (long long)user_id_to_i64(user_id));
    const char *account_type_s = account_type_as_str(account_type);

    const char *params[2] = { account_type_s, user_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT 1 FROM accounts WHERE account_type = $1 AND user_id = $2 LIMIT 1",
        2,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    PQclear(res);

    *out_exists = (rows > 0);
    if (err) *err = repo_error_ok();
    return true;
}
