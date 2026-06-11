#include "include/http_server.h"
#include <microhttpd.h>
#include "include/http_router.h"

/*
 * http_request_handler is registered here; AppState* travels as void* cls
 * and is cast back inside the router on every request.
 */
bool http_server_start(struct HttpServer *srv, AppState *state, unsigned short port) {
    // Start libmicrohttpd daemon with http_request_handler.
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
    // Stop daemon if running.
    if (srv->daemon) {
        MHD_stop_daemon(srv->daemon);
        srv->daemon = NULL;
    }
}
