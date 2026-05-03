#ifndef HTTP_USERS_H
#define HTTP_USERS_H

#include <microhttpd.h>
#include "http_state.h"
#include "ids.h"

enum MHD_Result http_users_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
);

enum MHD_Result http_users_get_user_with_accounts(
    AppState *state,
    struct MHD_Connection *conn,
    UserId path_user_id
);

enum MHD_Result http_users_not_found(struct MHD_Connection *conn);

#endif /* HTTP_USERS_H */