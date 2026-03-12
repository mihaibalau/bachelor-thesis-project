#ifndef C_DB_H
#define C_DB_H

#include <libpq-fe.h>
#include <stdbool.h>

#include "repo_error.h"

/* Thin wrapper over PGconn (libpq). */

typedef struct Db Db;

/* Connect using a libpq connection string, e.g.
 * "host=localhost port=5432 dbname=bank user=bank_app password=secret sslmode=prefer"
 */
bool db_connect(const char *conninfo, Db **out, RepoError *err);

/* Close connection and free Db. */
void db_close(Db *db);

/* Expose raw PGconn* for advanced usage if needed. */
PGconn *db_raw_connection(Db *db);

/* Execute a statement with text parameters.
 * Returns PGresult* on success (status PGRES_TUPLES_OK or PGRES_COMMAND_OK),
 * NULL on error and sets RepoError.
 * Caller must always call PQclear(result) on non-NULL PGresult.
 */
PGresult *db_exec_params(
    Db *db,
    const char *sql,
    int n_params,
    const char *const *param_values,
    RepoError *err
);

#endif //C_DB_H