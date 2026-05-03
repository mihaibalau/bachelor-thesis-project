#ifndef HTTP_ROUTER_H
#define HTTP_ROUTER_H

#include <microhttpd.h>
#include "http_state.h"

enum MHD_Result http_request_handler(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
);

#endif