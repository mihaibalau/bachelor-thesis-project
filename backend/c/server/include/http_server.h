#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "http_state.h"

struct HttpServer {
    struct MHD_Daemon *daemon;
};

bool http_server_start(struct HttpServer *srv, AppState *state, unsigned short port);
void http_server_stop(struct HttpServer *srv);

#endif