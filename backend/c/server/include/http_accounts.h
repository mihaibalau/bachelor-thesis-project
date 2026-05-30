#ifndef HTTP_ACCOUNTS_H
#define HTTP_ACCOUNTS_H

#include <microhttpd.h>
#include "http_state.h"

/*
 * Accounts HTTP routes
 *   POST /api/accounts              → open_account
 *   GET  /api/accounts/{id}         → get_account
 *   GET  /api/accounts/availability → get_availability
 */
enum MHD_Result http_accounts_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
);

#endif /* HTTP_ACCOUNTS_H */
