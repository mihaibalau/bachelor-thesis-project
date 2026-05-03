#ifndef HTTP_AUTH_H
#define HTTP_AUTH_H

#include <microhttpd.h>
#include <stdbool.h>
#include "jwt_utils.h"
#include "http_state.h"
#include "ids.h"
#include "service_error.h"

typedef struct AuthClaims {
    UserId sub;
} AuthClaims;

bool http_require_auth(
    struct MHD_Connection *conn,
    const AppState *state,
    AuthClaims *out_claims,
    ServiceError *err
);

#endif