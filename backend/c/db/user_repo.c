#include "include/user_repo.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../util/include/util_str.h"
#include "../domain/include/error.h"
#include "../domain/include/email.h"

/*
 * UserRepo = SQL access for users. Holds a Db* (shared PGconn).
 * Outbound: domain User*; inbound: domain User for inserts/updates.
 */

struct UserRepo {
    Db *db;
};

UserRepo *user_repo_new(Db *db) {
    if (!db) return NULL;
    UserRepo *r = (UserRepo *)malloc(sizeof *r);
    if (!r) return NULL;
    r->db = db;
    return r;
}

void user_repo_free(UserRepo *repo) {
    free(repo);
}

static int cmd_rows_affected(PGresult *res) {
    const char *s = PQcmdTuples(res);
    if (!s || *s == '\0') return 0;
    return atoi(s);
}

static bool parse_date_ymd(const char *s, struct tm *out) {
    if (!s || !out) return false;
    int y = 0, m = 0, d = 0;
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->tm_year = y - 1900;
    out->tm_mon  = m - 1;
    out->tm_mday = d;
    return true;
}

static bool format_date_ymd(const struct tm *src, char *buf, size_t buf_size) {
    if (!src || !buf || buf_size == 0) return false;
    size_t n = strftime(buf, buf_size, "%Y-%m-%d", src);
    return n > 0;
}

static bool user_repo_row_to_domain(
    PGresult *res,
    int row,
    User **out,
    RepoError *err
) {
    // Column order must match SELECT list in every query using this mapper.
    int col = 0;

    const char *id_s          = PQgetvalue(res, row, col++);
    const char *tag_s         = PQgetvalue(res, row, col++);
    const char *email_s       = PQgetvalue(res, row, col++);
    const char *first_name_s  = PQgetvalue(res, row, col++);
    const char *last_name_s   = PQgetvalue(res, row, col++);

    bool phone_is_null        = PQgetisnull(res, row, col);
    const char *phone_s       = phone_is_null ? NULL : PQgetvalue(res, row, col);
    col++;

    bool birth_is_null        = PQgetisnull(res, row, col);
    const char *birth_s       = birth_is_null ? NULL : PQgetvalue(res, row, col);
    col++;

    const char *password_s    = PQgetvalue(res, row, col++);

    int64_t id_v;
    if (!util_str_to_i64(id_s, &id_v)) {
        if (err) *err = repo_error_db("UserRepo: invalid id");
        return false;
    }
    UserId id = user_id_from_i64(id_v);

    DomainError derr;
    Email email;
    if (!email_try_create(email_s, &email, &derr)) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    struct tm birth_tm;
    const struct tm *birth_ptr = NULL;
    if (birth_s && birth_s[0] != '\0') {
        if (!parse_date_ymd(birth_s, &birth_tm)) {
            if (err) *err = repo_error_db("UserRepo: invalid birth_date format");
            return false;
        }
        birth_ptr = &birth_tm;
    }

    User *u = user_rehydrate(
        id,
        tag_s,
        &email,
        first_name_s,
        last_name_s,
        phone_s,
        birth_ptr,
        password_s,
        &derr
    );
    if (!u) {
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    if (err) *err = repo_error_ok();
    *out = u;
    return true;
}

// SELECT helpers

static bool user_repo_get_single_by_where(
    UserRepo *repo,
    const char *where_sql,
    const char *param,
    User **out,
    RepoError *err
) {
    if (!repo || !out || !where_sql || !param) {
        if (err) *err = repo_error_db("UserRepo::get: invalid arguments");
        return false;
    }

    const char *params[1] = { param };

    char sql[512];
    snprintf(sql, sizeof sql,
             "SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash "
             "FROM users WHERE %s", where_sql);

    PGresult *res = db_exec_params(
        repo->db,
        sql,
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
        if (err) *err = repo_error_not_found("user");
        return false;
    }

    bool ok = user_repo_row_to_domain(res, 0, out, err);
    PQclear(res);
    return ok;
}

// Public API

bool user_repo_get_by_id(UserRepo *repo, UserId id, User **out, RepoError *err) {
    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)user_id_to_i64(id));
    return user_repo_get_single_by_where(repo, "id = $1", id_buf, out, err);
}

bool user_repo_get_by_email(UserRepo *repo, const char *email_str, User **out, RepoError *err) {
    return user_repo_get_single_by_where(repo, "email = $1", email_str, out, err);
}

bool user_repo_get_by_tag(UserRepo *repo, const char *tag, User **out, RepoError *err) {
    return user_repo_get_single_by_where(repo, "tag = $1", tag, out, err);
}

bool user_repo_insert(UserRepo *repo, const User *user, UserId *out_id, RepoError *err) {
    // INSERT row; return generated id.
    if (!repo || !user || !out_id) {
        if (err) *err = repo_error_db("UserRepo::insert: invalid arguments");
        return false;
    }

    if (user_has_id(user)) {
        DomainError derr = domain_error_validation("Cannot insert a User that already has an id");
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    const char *tag          = user_tag(user);
    const Email *email_obj   = user_email(user);
    const char *email_s      = email_as_cstr(email_obj);
    const char *first_name   = user_first_name(user);
    const char *last_name    = user_last_name(user);
    const char *phone        = user_has_phone(user) ? user_phone(user) : "";

    char birth_buf[16];
    if (user_has_birth_date(user)) {
        struct tm tmv = user_birth_date(user);
        if (!format_date_ymd(&tmv, birth_buf, sizeof birth_buf)) {
            if (err) *err = repo_error_db("UserRepo::insert: failed to format birth_date");
            return false;
        }
    } else {
        birth_buf[0] = '\0';
    }

    const char *password_hash = user_password_hash(user);

    const char *params[7] = {
        tag,
        email_s,
        first_name,
        last_name,
        phone,
        birth_buf,
        password_hash
    };

    PGresult *res = db_exec_params(
        repo->db,
        "INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash) "
        "VALUES ($1, $2, $3, $4, NULLIF($5, ''), NULLIF($6, '')::date, $7) "
        "RETURNING id",
        7,
        params,
        err
    );
    if (!res) {
        return false;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        if (err) *err = repo_error_db("UserRepo::insert: expected 1 row");
        return false;
    }

    const char *id_s = PQgetvalue(res, 0, 0);
    int64_t id_v;
    if (!util_str_to_i64(id_s, &id_v)) {
        PQclear(res);
        if (err) *err = repo_error_db("UserRepo::insert: invalid id");
        return false;
    }

    PQclear(res);

    *out_id = user_id_from_i64(id_v);
    if (err) *err = repo_error_ok();
    return true;
}

bool user_repo_update(UserRepo *repo, const User *user, RepoError *err) {
    if (!repo || !user) {
        if (err) *err = repo_error_db("UserRepo::update: invalid arguments");
        return false;
    }

    if (!user_has_id(user)) {
        DomainError derr = domain_error_validation("Cannot update a User without an id");
        if (err) *err = repo_error_from_domain(&derr);
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)user_id_to_i64(user_id(user)));

    const char *tag          = user_tag(user);
    const Email *email_obj   = user_email(user);
    const char *email_s      = email_as_cstr(email_obj);
    const char *first_name   = user_first_name(user);
    const char *last_name    = user_last_name(user);
    const char *phone        = user_has_phone(user) ? user_phone(user) : "";

    char birth_buf[16];
    if (user_has_birth_date(user)) {
        struct tm tmv = user_birth_date(user);
        if (!format_date_ymd(&tmv, birth_buf, sizeof birth_buf)) {
            if (err) *err = repo_error_db("UserRepo::update: failed to format birth_date");
            return false;
        }
    } else {
        birth_buf[0] = '\0';
    }

    const char *password_hash = user_password_hash(user);

    const char *params[8] = {
        tag,
        email_s,
        first_name,
        last_name,
        phone,
        birth_buf,
        password_hash,
        id_buf
    };

    PGresult *res = db_exec_params(
        repo->db,
        "UPDATE users "
        "SET tag = $1, email = $2, first_name = $3, last_name = $4, "
        "    phone = NULLIF($5, ''), birth_date = NULLIF($6, '')::date, password_hash = $7 "
        "WHERE id = $8",
        8,
        params,
        err
    );
    if (!res) {
        return false;
    }

    int rows = cmd_rows_affected(res);
    PQclear(res);

    if (rows == 0) {
        if (err) *err = repo_error_not_found("user");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

bool user_repo_delete(UserRepo *repo, UserId id, RepoError *err) {
    if (!repo) {
        if (err) *err = repo_error_db("UserRepo::delete: invalid arguments");
        return false;
    }

    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "%lld", (long long)user_id_to_i64(id));
    const char *params[1] = { id_buf };

    PGresult *res = db_exec_params(
        repo->db,
        "DELETE FROM users WHERE id = $1",
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
        if (err) *err = repo_error_not_found("user");
        return false;
    }

    if (err) *err = repo_error_ok();
    return true;
}

// Tx control: forwarded to Db — same PGconn as account/transaction repos.
bool user_repo_begin(UserRepo *repo, RepoError *err) {
    if (!repo) {
        if (err) *err = repo_error_db("UserRepo::begin: invalid arguments");
        return false;
    }
    return db_begin(repo->db, err);
}

bool user_repo_commit(UserRepo *repo, RepoError *err) {
    if (!repo) {
        if (err) *err = repo_error_db("UserRepo::commit: invalid arguments");
        return false;
    }
    return db_commit(repo->db, err);
}

bool user_repo_rollback(UserRepo *repo) {
    if (!repo) return false;
    return db_rollback(repo->db);
}
