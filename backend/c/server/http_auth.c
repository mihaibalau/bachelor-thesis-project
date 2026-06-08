#include "include/http_auth.h"
#include "../util/include/log.h"
#include <string.h>

static enum MHD_Result get_authorization_header(
    void *cls,
    enum MHD_ValueKind kind,
    const char *key,
    const char *value
) {
    (void)kind;
    const char **out = (const char **)cls;
    if (0 == strcasecmp(key, "Authorization")) {
        *out = value;
        return MHD_NO;
    }
    return MHD_YES;
}

bool http_require_auth(
    struct MHD_Connection *conn,
    const AppState *state,
    AuthClaims *out_claims,
    ServiceError *err
) {
    // 1. Read Authorization header.
    const char *auth_header = NULL;
    MHD_get_connection_values(
        conn,
        MHD_HEADER_KIND,
        get_authorization_header,
        &auth_header
    );

    if (!auth_header) {
        LOG_DEBUG("auth", "missing Authorization header");
        if (err) *err = service_error_validation("missing Authorization header");
        return false;
    }

    // 2. Require Bearer scheme.
    const char prefix[] = "Bearer ";
    if (strncmp(auth_header, prefix, sizeof(prefix) - 1) != 0) {
        if (err) *err = service_error_validation("invalid Authorization scheme");
        return false;
    }

    // 3. Decode JWT and extract user id.
    //    Pass the token through as-is (no fixed-size copy) so an over-long token
    //    is validated in full and rejected, never silently truncated.
    const char *token_ptr = auth_header + sizeof(prefix) - 1;

    if (!jwt_decode_user_id(state->jwt_secret, token_ptr, &out_claims->sub, err)) {
        LOG_DEBUG("auth", "jwt validation failed");
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}