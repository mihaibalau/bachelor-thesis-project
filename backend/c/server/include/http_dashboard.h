#ifndef HTTP_DASHBOARD_H
#define HTTP_DASHBOARD_H

#include "http_state.h"
#include <microhttpd.h>

/*
 * Dashboard routes — mirrors backend/rust/src/server/routes/dashboard.rs:
 *   GET /api/dashboard → get_dashboard (auth required)
 */
enum MHD_Result http_dashboard_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method
);

#endif /* HTTP_DASHBOARD_H */
