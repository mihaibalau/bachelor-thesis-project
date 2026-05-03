#include "include/http_auth.h"
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
    const char *auth_header = NULL;
    MHD_get_connection_values(
        conn,
        MHD_HEADER_KIND,
        get_authorization_header,
        &auth_header
    );

    if (!auth_header) {
        if (err) *err = service_error_validation("missing Authorization header");
        return false;
    }

    const char prefix[] = "Bearer ";
    if (strncmp(auth_header, prefix, sizeof(prefix) - 1) != 0) {
        if (err) *err = service_error_validation("invalid Authorization scheme");
        return false;
    }

    const char *token_ptr = auth_header + sizeof(prefix) - 1;
    char token_copy[1024];
    strncpy(token_copy, token_ptr, sizeof(token_copy) - 1);
    token_copy[sizeof(token_copy) - 1] = '\0';

    if (!jwt_decode_user_id(state->jwt_secret, token_copy, &out_claims->sub, err)) {
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}