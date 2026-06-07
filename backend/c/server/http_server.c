#include "include/http_server.h"
#include <microhttpd.h>
#include "include/http_router.h"

bool http_server_start(struct HttpServer *srv, AppState *state, unsigned short port) {
    // 1. Start libmicrohttpd daemon with http_request_handler.
    srv->daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY,
        port,
        NULL, NULL,
        &http_request_handler, state,
        MHD_OPTION_END
    );
    return srv->daemon != NULL;
}

void http_server_stop(struct HttpServer *srv) {
    // 1. Stop daemon if running.
    if (srv->daemon) {
        MHD_stop_daemon(srv->daemon);
        srv->daemon = NULL;
    }
}