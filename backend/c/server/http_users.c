#include "include/http_users.h"
#include "include/http_error.h"
#include "include/http_auth.h"
#include "include/http_util.h"
#include "include/jwt_utils.h"
#include "service/include/user_service.h"
#include "domain/include/user.h"
#include "domain/include/email.h"
#include "domain/include/account.h"
#include "domain/include/account_type.h"
#include "domain/include/currency.h"
#include "domain/include/iban.h"
#include "domain/include/ids.h"

#include <microhttpd.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
 * Users HTTP routes — thin layer. Body buffering, CORS, JSON/error response
 * helpers live in http_util.c and are shared with every other route file.
 *
 * Mirrors backend/rust/src/server/routes/users.rs:
 *   POST /api/users         → register_user
 *   POST /api/users/login   → login_user
 *   GET  /api/users/{id}    → get_user_with_accounts  (auth required)
 */

/* ── POST /api/users  (register) ──────────────────────────────────────────── */

static enum MHD_Result handle_register(
    AppState *state,
    struct MHD_Connection *conn,
    const char *body
) {
    json_error_t jerr;
    json_t *root = json_loads(body, 0, &jerr);
    if (!root) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"bad_request\",\"message\":\"invalid JSON body\"}");
    }

    const char *tag        = json_string_value(json_object_get(root, "tag"));
    const char *email      = json_string_value(json_object_get(root, "email"));
    const char *first_name = json_string_value(json_object_get(root, "first_name"));
    const char *last_name  = json_string_value(json_object_get(root, "last_name"));
    const char *password   = json_string_value(json_object_get(root, "password"));
    const char *phone      = json_string_value(json_object_get(root, "phone"));       /* opt */
    const char *birth_date = json_string_value(json_object_get(root, "birth_date")); /* opt */

    if (!tag || !email || !first_name || !last_name || !password) {
        json_decref(root);
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"tag, email, first_name, last_name and password are required\"}");
    }

    /* birth_date "YYYY-MM-DD" → struct tm */
    struct tm birth_date_tm  = {0};
    struct tm *birth_date_ptr = NULL;
    if (birth_date) {
        int y, m, d;
        if (sscanf(birth_date, "%d-%d-%d", &y, &m, &d) == 3) {
            birth_date_tm.tm_year = y - 1900;
            birth_date_tm.tm_mon  = m - 1;
            birth_date_tm.tm_mday = d;
            birth_date_ptr = &birth_date_tm;
        }
    }

    RegisterUserCommand cmd = {
        .tag            = tag,
        .email          = email,
        .first_name     = first_name,
        .last_name      = last_name,
        .phone_opt      = phone,
        .birth_date_opt = birth_date_ptr,
        .password       = password,   /* hashing happens inside the service */
    };

    UserId new_id;
    ServiceError serr;
    bool ok = user_service_register_user(state->user_svc, &cmd, &new_id, &serr);
    json_decref(root);

    if (!ok) return http_send_service_error(conn, &serr);

    char resp[64];
    snprintf(resp, sizeof resp, "{\"user_id\":%lld}", (long long)new_id.value);
    /* Rust returns 200 OK (axum Json default) for register; match it. */
    return http_send_json(conn, MHD_HTTP_OK, resp);
}

/* ── POST /api/users/login ────────────────────────────────────────────────── */

static enum MHD_Result handle_login(
    AppState *state,
    struct MHD_Connection *conn,
    const char *body
) {
    json_error_t jerr;
    json_t *root = json_loads(body, 0, &jerr);
    if (!root) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"bad_request\",\"message\":\"invalid JSON body\"}");
    }

    const char *email    = json_string_value(json_object_get(root, "email"));
    const char *password = json_string_value(json_object_get(root, "password"));

    if (!email || !password) {
        json_decref(root);
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"email and password are required\"}");
    }

    LoginUserCommand cmd = {
        .email    = email,
        .password = password,
    };

    LoginUserResult result;
    ServiceError serr;

    bool ok = user_service_login_user(state->user_svc, &cmd, &result, &serr);
    json_decref(root);

    if (!ok) {
        return http_send_service_error(conn, &serr);
    }

    /* Generate JWT */
    char token[512];
    ServiceError jwt_err;

    if (!jwt_encode_user_id(state->jwt_secret,
                            result.user_id.value,
                            result.tag,
                            token,
                            sizeof token,
                            &jwt_err)) {
        return http_send_service_error(conn, &jwt_err);
    }

    char resp[640];
    snprintf(resp, sizeof resp,
             "{\"token\":\"%s\",\"user_id\":%lld}",
             token, (long long)result.user_id.value);

    return http_send_json(conn, MHD_HTTP_OK, resp);
}

/* ── Public ──────────────────────────────────────────────────────────────── */

enum MHD_Result http_users_not_found(struct MHD_Connection *conn) {
    return http_send_not_found(conn);
}

/* ── Dispatcher ──────────────────────────────────────────────────────────── */

enum MHD_Result http_users_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
) {

    if (strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0) {
        return http_send_empty(conn, MHD_HTTP_NO_CONTENT);
    }

    /* GET /api/users/{id} — no body */
    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if (subpath && subpath[0] == '/' && subpath[1] != '\0') {
            char *endptr = NULL;
            long id_long = strtol(subpath + 1, &endptr, 10);
            if (*endptr != '\0' || id_long <= 0) {
                return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
                    "{\"status\":422,\"code\":\"validation_error\","
                    "\"message\":\"invalid user id\"}");
            }
            UserId uid = { .value = (int64_t)id_long };
            return http_users_get_user_with_accounts(state, conn, uid);
        }
        return http_users_not_found(conn);
    }

    /* POST — accumulate the body across multiple libmicrohttpd calls */
    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {

        if (*con_cls == NULL) {
            BodyBuffer *bb = body_buffer_new();
            if (!bb) return MHD_NO;
            *con_cls = bb;
            return MHD_YES;
        }

        BodyBuffer *bb = (BodyBuffer *)*con_cls;

        if (*upload_data_size > 0) {
            if (!body_buffer_append(bb, upload_data, *upload_data_size)) {
                body_buffer_free(bb);
                *con_cls = NULL;
                return MHD_NO;
            }
            *upload_data_size = 0;
            return MHD_YES;
        }

        const char *body = bb->buf ? bb->buf : "";
        enum MHD_Result result;

        if (!subpath || strcmp(subpath, "") == 0 || strcmp(subpath, "/") == 0) {
            result = handle_register(state, conn, body);
        } else if (strcmp(subpath, "/login") == 0) {
            result = handle_login(state, conn, body);
        } else {
            result = http_users_not_found(conn);
        }

        body_buffer_free(bb);
        *con_cls = NULL;
        return result;
    }

    return http_users_not_found(conn);
}

/* ── GET /api/users/{id} ─────────────────────────────────────────────────── */

enum MHD_Result http_users_get_user_with_accounts(
    AppState *state,
    struct MHD_Connection *conn,
    UserId path_user_id
) {
    ServiceError serr;
    AuthClaims claims;

    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\","
            "\"message\":\"missing or invalid token\"}");
    }

    /* echivalent Rust: if claims.sub != id { return Err(Forbidden) } */
    if (claims.sub.value != path_user_id.value) {
        ServiceError forbidden = service_error_forbidden();
        return http_send_service_error(conn, &forbidden);
    }

    UserWithAccounts dto;
    if (!user_service_get_user_with_accounts(
            state->user_svc, path_user_id, &dto, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    /* ── user object ──────────────────────────────────────────────────────── */

    json_t *user_obj = json_object();

    json_object_set_new(user_obj, "id",
        json_integer((json_int_t)user_id(dto.user).value));
    json_object_set_new(user_obj, "tag",
        json_string(user_tag(dto.user)));
    json_object_set_new(user_obj, "email",
        json_string(email_as_cstr(user_email(dto.user))));
    json_object_set_new(user_obj, "first_name",
        json_string(user_first_name(dto.user)));
    json_object_set_new(user_obj, "last_name",
        json_string(user_last_name(dto.user)));

    /* phone — optional */
    if (user_has_phone(dto.user)) {
        json_object_set_new(user_obj, "phone",
            json_string(user_phone(dto.user)));
    } else {
        json_object_set_new(user_obj, "phone", json_null());
    }

    /* birth_date — optional, serialized as "YYYY-MM-DD" */
    if (user_has_birth_date(dto.user)) {
        struct tm bd = user_birth_date(dto.user);
        char date_str[11];
        strftime(date_str, sizeof date_str, "%Y-%m-%d", &bd);
        json_object_set_new(user_obj, "birth_date", json_string(date_str));
    } else {
        json_object_set_new(user_obj, "birth_date", json_null());
    }

    /* ── accounts array ───────────────────────────────────────────────────── */

    json_t *accs = json_array();
    for (size_t i = 0; i < dto.account_count; ++i) {
        Account *a  = dto.accounts[i];
        json_t  *ao = json_object();

        json_object_set_new(ao, "id",
            json_integer((json_int_t)account_id(a).value));
        json_object_set_new(ao, "account_type",
            json_string(account_type_as_str(account_type_get(a))));
        json_object_set_new(ao, "currency",
            json_string(currency_as_str(account_currency(a))));
        json_object_set_new(ao, "balance_cents",
            json_integer((json_int_t)account_balance_cents(a)));
        json_object_set_new(ao, "iban",
            json_string(iban_as_cstr(account_iban(a))));

        json_array_append_new(accs, ao);
    }

    /* ── root → serialize → send ──────────────────────────────────────────── */

    json_t *root = json_object();
    json_object_set_new(root, "user", user_obj);
    json_object_set_new(root, "accounts", accs);

    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (json_str) {
        ret = http_send_json(conn, MHD_HTTP_OK, json_str);
        free(json_str);
    } else {
        ServiceError internal = service_error_internal("json serialization failed");
        ret = http_send_service_error(conn, &internal);
    }

    /* cleanup — we own dto after the service call */
    user_free(dto.user);
    for (size_t i = 0; i < dto.account_count; ++i) account_free(dto.accounts[i]);
    free(dto.accounts);

    return ret;
}
