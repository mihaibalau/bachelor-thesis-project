#ifndef HTTP_AFFILIATES_H
#define HTTP_AFFILIATES_H

#include <microhttpd.h>
#include "http_state.h"

/*
 * Affiliates HTTP routes
 *   GET    /api/affiliates                  → list_affiliates
 *   POST   /api/affiliates                  → create_affiliate
 *   GET    /api/affiliates/{sub_account_id} → get_affiliate
 *   PATCH  /api/affiliates/{sub_account_id} → update_affiliate_nickname
 *   DELETE /api/affiliates/{sub_account_id} → delete_affiliate
 *   POST   /api/affiliates/resolve-target   → resolve_affiliate_target
 */
enum MHD_Result http_affiliates_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
);

#endif /* HTTP_AFFILIATES_H */
