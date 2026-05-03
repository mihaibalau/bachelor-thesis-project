#include "include/http_router.h"
#include <string.h>
#include "include/http_users.h"

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

    if (strncmp(url, "/api/users", 10) == 0) {
        return http_users_dispatch(
            state, connection,
            url + 10,
            method,
            upload_data, upload_data_size, con_cls
        );
    }

    return http_users_not_found(connection);
}