#include "include/db.h"

#include <stdlib.h>
#include <string.h>

struct Db {
    PGconn *conn;
};

bool db_connect(const char *conninfo, Db **out, RepoError *err) {
    // 1. Allocate Db and connect via libpq.
    if (!conninfo || !out) {
        if (err) *err = repo_error_db("Db: invalid arguments");
        return false;
    }

    Db *db = (Db *)malloc(sizeof *db);
    if (!db) {
        if (err) *err = repo_error_db("Db: out of memory");
        return false;
    }

    db->conn = PQconnectdb(conninfo);
    if (PQstatus(db->conn) != CONNECTION_OK) {
        const char *msg = PQerrorMessage(db->conn);
        if (err) *err = repo_error_db(msg);
        PQfinish(db->conn);
        free(db);
        return false;
    }

    if (err) *err = repo_error_ok();
    *out = db;
    return true;
}

void db_close(Db *db) {
    if (!db) return;
    if (db->conn) {
        PQfinish(db->conn);
        db->conn = NULL;
    }
    free(db);
}

PGconn *db_raw_connection(Db *db) {
    return db ? db->conn : NULL;
}

PGresult *db_exec_params(
    Db *db,
    const char *sql,
    int n_params,
    const char *const *param_values,
    RepoError *err
) {
    if (!db || !sql) {
        if (err) *err = repo_error_db("Db: invalid arguments");
        return NULL;
    }

    PGresult *res = PQexecParams(
        db->conn,
        sql,
        n_params,
        NULL,
        param_values,
        NULL,
        NULL,
        0
    );

    if (!res) {
        if (err) *err = repo_error_db("Db: PQexecParams returned NULL");
        return NULL;
    }

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const char *msg = PQresultErrorMessage(res);
        if (!msg || msg[0] == '\0') msg = PQerrorMessage(db->conn);
        if (err) *err = repo_error_db(msg);
        PQclear(res);
        return NULL;
    }

    if (err) *err = repo_error_ok();
    return res;
}

// Run a parameter-less control statement (BEGIN/COMMIT/ROLLBACK) via PQexec.
static bool db_exec_simple(Db *db, const char *sql, RepoError *err) {
    if (!db || !db->conn) {
        if (err) *err = repo_error_db("Db: invalid arguments");
        return false;
    }
    PGresult *res = PQexec(db->conn, sql);
    if (!res) {
        if (err) *err = repo_error_db("Db: PQexec returned NULL");
        return false;
    }
    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        const char *msg = PQresultErrorMessage(res);
        if (!msg || msg[0] == '\0') msg = PQerrorMessage(db->conn);
        if (err) *err = repo_error_db(msg);
        PQclear(res);
        return false;
    }
    PQclear(res);
    if (err) *err = repo_error_ok();
    return true;
}

bool db_begin(Db *db, RepoError *err) {
    return db_exec_simple(db, "BEGIN", err);
}

bool db_commit(Db *db, RepoError *err) {
    return db_exec_simple(db, "COMMIT", err);
}

bool db_rollback(Db *db) {
    // Best-effort: never overwrites the caller's original error.
    return db_exec_simple(db, "ROLLBACK", NULL);
}
