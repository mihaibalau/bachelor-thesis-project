#include "include/http_router.h"
#include <string.h>
#include "include/http_users.h"
#include "include/http_accounts.h"
#include "include/http_affiliates.h"
#include "include/http_transactions.h"
#include "include/http_util.h"

/*
 * Top-level URL dispatch.
 *   /api/users        → http_users_dispatch
 *   /api/accounts     → http_accounts_dispatch
 *   /api/affiliates   → http_affiliates_dispatch
 *   /api/transactions → http_transactions_dispatch
 *
 * Each group receives the URL with its prefix stripped (the "subpath"),
 * exactly like axum's `.nest()`.
 */

/* Match `prefix` at the start of `url` only at a path boundary (next char is
 * '\0' or '/'), and return the remaining subpath. Prevents "/api/accountsX"
 * from matching "/api/accounts". */
static const char *match_prefix(const char *url, const char *prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(url, prefix, plen) != 0) return NULL;
    char next = url[plen];
    if (next != '\0' && next != '/') return NULL;
    return url + plen;
}

enum MHD_Result http_request_handler(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
) {
    (void)version;
    AppState *state = (AppState *)cls;

    const char *subpath;

    if ((subpath = match_prefix(url, "/api/users")) != NULL) {
        return http_users_dispatch(
            state, connection, subpath, method,
            upload_data, upload_data_size, con_cls);
    }

    if ((subpath = match_prefix(url, "/api/accounts")) != NULL) {
        return http_accounts_dispatch(
            state, connection, subpath, method,
            upload_data, upload_data_size, con_cls);
    }

    if ((subpath = match_prefix(url, "/api/affiliates")) != NULL) {
        return http_affiliates_dispatch(
            state, connection, subpath, method,
            upload_data, upload_data_size, con_cls);
    }

    if ((subpath = match_prefix(url, "/api/transactions")) != NULL) {
        return http_transactions_dispatch(
            state, connection, subpath, method,
            upload_data, upload_data_size, con_cls);
    }

    return http_send_not_found(connection);
}
