#include "include/affiliate_repo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../util/include/util_str.h"
#include "../domain/include/error.h"

struct AffiliateRepo {
    Db *db; /* non-owning */
};

AffiliateRepo *affiliate_repo_new(Db *db) {
    if (!db) return NULL;
    AffiliateRepo *r = (AffiliateRepo *)malloc(sizeof *r);
    if (!r) return NULL;
    r->db = db;
    return r;
}

void affiliate_repo_free(AffiliateRepo *repo) {
    free(repo);
}

static int cmd_rows_affected(PGresult *res) {
    const char *s = PQcmdTuples(res);
    if (!s || *s == '\0') return 0;
    return atoi(s);
}

/* Row mapping -> Affiliate*. */
static bool affiliate_repo_row_to_domain(
    PGresult *res,
    int row,
    Affiliate **out,
    RepoError *err
) {
    int col = 0;

    const char *owner_id_s = PQgetvalue(res, row, col++);
    const char *sub_id_s   = PQgetvalue(res, row, col++);
    const char *nick_s     = PQgetvalue(res, row, col++);

    int64_t owner_v, sub_v;
    if (!util_str_to_i64(owner_id_s, &owner_v) ||
        !util_str_to_i64(sub_id_s, &sub_v)) {
        if (err) *err = repo_error_db("AffiliateRepo: invalid integer in result");
        return false;
    }

    UserId    owner_id = user_id_from_i64(owner_v);
    AccountId sub_id   = account_id_from_i64(sub_v);

    DomainError derr;
    Affiliate *a = affiliate_new(owner_id, sub_id, nick_s, &derr);
    if (!a) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    if (err) *err = repo_error_ok();
    *out = a;
    return true;
}

/* Public API */

bool affiliate_repo_get(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    Affiliate **out,
    RepoError *err
) {
    if (!repo || !out) {
        if (err) *err = repo_error_db("AffiliateRepo::get: invalid arguments");
        return false;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(owner_user_id));

    char sub_buf[32];
    snprintf(sub_buf, sizeof sub_buf, "%lld",
             (long long)account_id_to_i64(recipient_sub_account_id));

    const char *params[2] = { owner_buf, sub_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT owner_user_id, recipient_sub_account_id, nickname "
        "FROM affiliates "
        "WHERE owner_user_id = $1 AND recipient_sub_account_id = $2",
        2,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    if (rows == 0) {
        PQclear(res);
        if (err) *err = repo_error_not_found("affiliate");
        return false;
    }

    bool ok = affiliate_repo_row_to_domain(res, 0, out, err);
    PQclear(res);
    return ok;
}

bool affiliate_repo_list_for_owner(
    AffiliateRepo *repo,
    UserId owner_user_id,
    Affiliate ***out_affiliates,
    size_t *out_count,
    RepoError *err
) {
    if (!repo || !out_affiliates || !out_count) {
        if (err) *err = repo_error_db("AffiliateRepo::list_for_owner: invalid arguments");
        return false;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(owner_user_id));

    const char *params[1] = { owner_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT owner_user_id, recipient_sub_account_id, nickname "
        "FROM affiliates "
        "WHERE owner_user_id = $1 "
        "ORDER BY recipient_sub_account_id",
        1,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = PQntuples(res);
    if (rows < 0) rows = 0;

    Affiliate **arr = NULL;
    if (rows > 0) {
        arr = (Affiliate **)calloc((size_t)rows, sizeof(Affiliate *));
        if (!arr) {
            PQclear(res);
            if (err) *err = repo_error_db("AffiliateRepo::list_for_owner: out of memory");
            return false;
        }
    }

    bool ok = true;
    for (int i = 0; i < rows; ++i) {
        Affiliate *a = NULL;
        if (!affiliate_repo_row_to_domain(res, i, &a, err)) {
            ok = false;
            for (int j = 0; j < i; ++j) {
                affiliate_free(arr[j]);
            }
            free(arr);
            break;
        }
        arr[i] = a;
    }

    PQclear(res);

    if (!ok) return false;

    *out_affiliates = arr;
    *out_count = (size_t)rows;
    if (err) *err = repo_error_ok();
    return true;
}

bool affiliate_repo_insert(
    AffiliateRepo *repo,
    const Affiliate *affiliate,
    RepoError *err
) {
    if (!repo || !affiliate) {
        if (err) *err = repo_error_db("AffiliateRepo::insert: invalid arguments");
        return false;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(affiliate_owner_user_id(affiliate)));

    char sub_buf[32];
    snprintf(sub_buf, sizeof sub_buf, "%lld",
             (long long)account_id_to_i64(affiliate_recipient_sub_account_id(affiliate)));

    const char *nickname = affiliate_nickname(affiliate);

    const char *params[3] = {
        owner_buf,
        sub_buf,
        nickname
    };

    PGresult *res = db_exec_params(
        repo->db,
        "INSERT INTO affiliates (owner_user_id, recipient_sub_account_id, nickname) "
        "VALUES ($1, $2, $3)",
        3,
        params,
        err
    );
    if (!res) {
        return false;
    }

    PQclear(res);
    if (err) *err = repo_error_ok();
    return true;
}

bool affiliate_repo_update_nickname(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    RepoError *err
) {
    if (!repo || !nickname) {
        if (err) *err = repo_error_db("AffiliateRepo::update_nickname: invalid arguments");
        return false;
    }

    DomainError derr;
    Affiliate *tmp = affiliate_new(owner_user_id, recipient_sub_account_id, nickname, &derr);
    if (!tmp) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }
    affiliate_free(tmp);

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(owner_user_id));

    char sub_buf[32];
    snprintf(sub_buf, sizeof sub_buf, "%lld",
             (long long)account_id_to_i64(recipient_sub_account_id));

    const char *params[3] = {
        nickname,
        owner_buf,
        sub_buf
    };

    PGresult *res = db_exec_params(
        repo->db,
        "UPDATE affiliates "
        "SET nickname = $1 "
        "WHERE owner_user_id = $2 AND recipient_sub_account_id = $3",
        3,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = cmd_rows_affected(res);
    PQclear(res);

    if (rows == 0) {
        if (err) *err = repo_error_not_found("affiliate");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

bool affiliate_repo_delete(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    RepoError *err
) {
    if (!repo) {
        if (err) *err = repo_error_db("AffiliateRepo::delete: invalid arguments");
        return false;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(owner_user_id));

    char sub_buf[32];
    snprintf(sub_buf, sizeof sub_buf, "%lld",
             (long long)account_id_to_i64(recipient_sub_account_id));

    const char *params[2] = { owner_buf, sub_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "DELETE FROM affiliates "
        "WHERE owner_user_id = $1 AND recipient_sub_account_id = $2",
        2,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = cmd_rows_affected(res);
    PQclear(res);

    if (rows == 0) {
        if (err) *err = repo_error_not_found("affiliate");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

bool affiliate_repo_exists(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    bool *out_exists,
    RepoError *err
) {
    if (!repo || !out_exists) {
        if (err) *err = repo_error_db("AffiliateRepo::exists: invalid arguments");
        return false;
    }

    char owner_buf[32];
    snprintf(owner_buf, sizeof owner_buf, "%lld",
             (long long)user_id_to_i64(owner_user_id));

    char sub_buf[32];
    snprintf(sub_buf, sizeof sub_buf, "%lld",
             (long long)account_id_to_i64(recipient_sub_account_id));

    const char *params[2] = { owner_buf, sub_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "SELECT 1 FROM affiliates "
        "WHERE owner_user_id = $1 AND recipient_sub_account_id = $2 "
        "LIMIT 1",
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
