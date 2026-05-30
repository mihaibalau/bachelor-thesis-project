#include "include/http_accounts.h"
#include "include/http_util.h"
#include "include/http_auth.h"
#include "service/include/account_service.h"
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
#include <errno.h>

/*
 * Accounts routes. This layer only parses/serializes and forwards to
 * AccountService — every business rule (IBAN generation, type/currency
 * parsing, uniqueness, availability) lives in the service.
 */

// ── Serialization ──────────────────────────────────────────────────────

// Builds the AccountResponse JSON object (caller owns the returned json_t).
static json_t *account_to_json(const Account *a) {
    json_t *o = json_object();
    json_object_set_new(o, "id",            json_integer((json_int_t)account_id(a).value));
    json_object_set_new(o, "account_type",  json_string(account_type_as_str(account_type_get(a))));
    json_object_set_new(o, "currency",      json_string(currency_as_str(account_currency(a))));
    json_object_set_new(o, "balance_cents", json_integer((json_int_t)account_balance_cents(a)));
    json_object_set_new(o, "iban",          json_string(iban_as_cstr(account_iban(a))));
    return o;
}

static enum MHD_Result send_account(struct MHD_Connection *conn, const Account *a) {
    json_t *o = account_to_json(a);
    char *s = json_dumps(o, JSON_COMPACT);
    json_decref(o);
    enum MHD_Result ret;
    if (s) {
        ret = http_send_json(conn, MHD_HTTP_OK, s);
        free(s);
    } else {
        ServiceError e = service_error_internal("json serialization failed");
        ret = http_send_service_error(conn, &e);
    }
    return ret;
}

// ── POST /api/accounts ──────────────────────────────────────────────────

static enum MHD_Result handle_open_account(
    AppState *state,
    struct MHD_Connection *conn,
    const char *body
) {
    ServiceError serr;
    AuthClaims claims;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\","
            "\"message\":\"missing or invalid token\"}");
    }

    json_error_t jerr;
    json_t *root = json_loads(body, 0, &jerr);
    if (!root) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"bad_request\",\"message\":\"invalid JSON body\"}");
    }

    const char *account_type = json_string_value(json_object_get(root, "account_type"));
    const char *currency      = json_string_value(json_object_get(root, "currency"));
    json_t *bal_j             = json_object_get(root, "initial_balance_cents");

    if (!account_type || !currency || !json_is_integer(bal_j)) {
        json_decref(root);
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"account_type, currency and initial_balance_cents are required\"}");
    }

    int64_t initial_balance_cents = (int64_t)json_integer_value(bal_j);

    // The auth user owns the new account (Rust: UserId::from(claims.sub)).
    AccountId new_id;
    bool ok = account_service_open_account_raw(
        state->account_svc, claims.sub,
        account_type, currency, initial_balance_cents,
        &new_id, &serr);
    json_decref(root);

    if (!ok) return http_send_service_error(conn, &serr);

    // Load the freshly created account and return it.
    Account *account = NULL;
    if (!account_service_get_account(state->account_svc, new_id, &account, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    enum MHD_Result ret = send_account(conn, account);
    account_free(account);
    return ret;
}

// ── GET /api/accounts/{id} ────────────────────────────────────────────────

static enum MHD_Result handle_get_account(
    AppState *state,
    struct MHD_Connection *conn,
    AccountId account_id_path
) {
    ServiceError serr;
    AuthClaims claims;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\","
            "\"message\":\"missing or invalid token\"}");
    }

    Account *account = NULL;
    if (!account_service_get_account(state->account_svc, account_id_path, &account, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    // Ownership check (Rust: if account.user_id != claims.sub → not_found).
    if (account_user_id(account).value != claims.sub.value) {
        account_free(account);
        ServiceError nf = service_error_not_found("account");
        return http_send_service_error(conn, &nf);
    }

    enum MHD_Result ret = send_account(conn, account);
    account_free(account);
    return ret;
}

// ── GET /api/accounts/availability ───────────────────────────────────────

static enum MHD_Result handle_get_availability(
    AppState *state,
    struct MHD_Connection *conn
) {
    ServiceError serr;
    AuthClaims claims;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\","
            "\"message\":\"missing or invalid token\"}");
    }

    AccountAvailability availability;
    if (!account_service_get_account_availability(
            state->account_svc, claims.sub, &availability, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    size_t type_count = 0, curr_count = 0;
    const AccountType *types      = account_type_all(&type_count);
    const Currency    *currencies = currency_all(&curr_count);

    json_t *types_arr = json_array();
    for (size_t t = 0; t < type_count; ++t) {
        AccountType at = types[t];

        json_t *curr_arr = json_array();
        bool has_any = false;
        for (size_t c = 0; c < curr_count; ++c) {
            Currency cur = currencies[c];
            bool avail = availability.available[at][cur];
            if (avail) has_any = true;

            json_t *co = json_object();
            json_object_set_new(co, "currency",  json_string(currency_as_str(cur)));
            json_object_set_new(co, "available", json_boolean(avail));
            json_array_append_new(curr_arr, co);
        }

        json_t *to = json_object();
        json_object_set_new(to, "account_type",     json_string(account_type_as_str(at)));
        json_object_set_new(to, "has_any_available", json_boolean(has_any));
        json_object_set_new(to, "currencies",        curr_arr);
        json_array_append_new(types_arr, to);
    }

    json_t *root = json_object();
    json_object_set_new(root, "types", types_arr);

    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) {
        ret = http_send_json(conn, MHD_HTTP_OK, s);
        free(s);
    } else {
        ServiceError e = service_error_internal("json serialization failed");
        ret = http_send_service_error(conn, &e);
    }
    return ret;
}

// ── Dispatcher ─────────────────────────────────────────────────────────

enum MHD_Result http_accounts_dispatch(
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

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if (subpath && strcmp(subpath, "/availability") == 0) {
            return handle_get_availability(state, conn);
        }
        if (subpath && subpath[0] == '/' && subpath[1] != '\0') {
            const char *id_str = subpath + 1;
            char *endptr = NULL;
            errno = 0;
            long long id_ll = strtoll(id_str, &endptr, 10);
            /* A non-numeric or out-of-range id is a malformed path parameter.
             * axum's Path<i64> extractor rejects these with 400, so we mirror
             * that here (previously 422). A syntactically valid i64 — including
             * 0 or negatives — flows to the handler, which returns 404 when the
             * account does not exist, exactly like the Rust handler. */
            if (endptr == id_str || *endptr != '\0' || errno == ERANGE) {
                return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
                    "{\"status\":400,\"code\":\"bad_request\","
                    "\"message\":\"invalid account id\"}");
            }
            AccountId aid = { .value = (int64_t)id_ll };
            return handle_get_account(state, conn, aid);
        }
        return http_send_not_found(conn);
    }

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
            result = handle_open_account(state, conn, body);
        } else {
            result = http_send_not_found(conn);
        }
        body_buffer_free(bb);
        *con_cls = NULL;
        return result;
    }

    return http_send_not_found(conn);
}
