#include "include/http_router.h"
#include <string.h>
#include "include/http_users.h"
#include "include/http_accounts.h"
#include "include/http_affiliates.h"
#include "include/http_transactions.h"
#include "include/http_dashboard.h"
#include "include/http_util.h"
#include "../util/include/log.h"

/*
 * Top-level HTTP router: /health is bare; /api/* strips prefix and
 * forwards subpath to a module dispatcher (Axum .nest() equivalent).
 */

// Match prefix at a path boundary; return remaining subpath.
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

    // 1. Log non-OPTIONS requests.
    if (strcmp(method, "OPTIONS") != 0) {
        LOG_INFO("http_request", "method=%s path=%s", method, url);
    }

    // 2. Unauthenticated health probe (outside the /api group) for load balancers.
    if (strcmp(url, "/health") == 0 && strcmp(method, "GET") == 0) {
        return http_send_json(connection, MHD_HTTP_OK, "{\"status\":\"ok\"}");
    }

    // 3. Match route group and delegate to sub-dispatcher.
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

    if ((subpath = match_prefix(url, "/api/dashboard")) != NULL) {
        return http_dashboard_dispatch(state, connection, subpath, method);
    }

    return http_send_not_found(connection);
}
