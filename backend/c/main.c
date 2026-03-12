// main.c
#include <stdio.h>
#include <stdlib.h>

#include "db/include/db.h"
#include "db/include/repo_error.h"

int main(void) {
    const char *conninfo = getenv("DB_CONN");
    if (!conninfo) {
        conninfo = "host=localhost port=5432 dbname=gentlix_bank user=mihai password=admingentlix01 sslmode=disable";
    }

    RepoError rerr;
    Db *db = NULL;

    if (!db_connect(conninfo, &db, &rerr)) {
        fprintf(stderr, "Failed to connect to DB: %s\n", rerr.message);
        return 1;
    }

    printf("Connected to PostgreSQL.\n");

    PGresult *res = db_exec_params(
        db,
        "SELECT id, tag, email FROM users ORDER BY id LIMIT 10",
        0, NULL, &rerr
    );
    if (!res) {
        fprintf(stderr, "Query users failed: %s\n", rerr.message);
        db_close(db);
        return 1;
    }

    printf("\n=== USERS (%d rows) ===\n", PQntuples(res));
    for (int i = 0; i < PQntuples(res); ++i) {
        printf("  #%s  tag=%-20s  email=%s\n",
               PQgetvalue(res, i, 0),
               PQgetvalue(res, i, 1),
               PQgetvalue(res, i, 2));
    }
    PQclear(res);

    res = db_exec_params(
        db,
        "SELECT id, user_id, account_type, currency, balance_cents FROM accounts ORDER BY id LIMIT 10",
        0, NULL, &rerr
    );
    if (!res) {
        fprintf(stderr, "Query accounts failed: %s\n", rerr.message);
        db_close(db);
        return 1;
    }

    printf("\n=== ACCOUNTS (%d rows) ===\n", PQntuples(res));
    for (int i = 0; i < PQntuples(res); ++i) {
        printf("  #%s  user_id=%s  type=%-10s  currency=%s  balance=%s cents\n",
               PQgetvalue(res, i, 0),
               PQgetvalue(res, i, 1),
               PQgetvalue(res, i, 2),
               PQgetvalue(res, i, 3),
               PQgetvalue(res, i, 4));
    }
    PQclear(res);

    res = db_exec_params(
        db,
        "SELECT id, from_account_id, to_account_id, transaction_type, value_cents FROM transactions ORDER BY id LIMIT 10",
        0, NULL, &rerr
    );
    if (!res) {
        fprintf(stderr, "Query transactions failed: %s\n", rerr.message);
        db_close(db);
        return 1;
    }

    printf("\n=== TRANSACTIONS (%d rows) ===\n", PQntuples(res));
    for (int i = 0; i < PQntuples(res); ++i) {
        printf("  #%s  from=%s  to=%s  type=%-10s  value=%s cents\n",
               PQgetvalue(res, i, 0),
               PQgetvalue(res, i, 1),
               PQgetvalue(res, i, 2),
               PQgetvalue(res, i, 3),
               PQgetvalue(res, i, 4));
    }
    PQclear(res);

    res = db_exec_params(
        db,
        "SELECT owner_user_id, recipient_sub_account_id, nickname FROM affiliates ORDER BY owner_user_id LIMIT 10",
        0, NULL, &rerr
    );
    if (!res) {
        fprintf(stderr, "Query affiliates failed: %s\n", rerr.message);
        db_close(db);
        return 1;
    }

    printf("\n=== AFFILIATES (%d rows) ===\n", PQntuples(res));
    for (int i = 0; i < PQntuples(res); ++i) {
        printf("  owner=%s  sub_account=%s  nickname=%s\n",
               PQgetvalue(res, i, 0),
               PQgetvalue(res, i, 1),
               PQgetvalue(res, i, 2));
    }
    PQclear(res);

    db_close(db);
    printf("\nAll queries OK.\n");
    return 0;
}
