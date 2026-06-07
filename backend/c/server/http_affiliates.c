#include "include/http_affiliates.h"
#include "include/http_util.h"
#include "include/http_auth.h"
#include "service/include/affiliate_service.h"
#include "domain/include/ids.h"

#include <microhttpd.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Affiliates routes: parse/serialize only; views/search in AffiliateService.

static enum MHD_Result auth_401(struct MHD_Connection *conn) {
    return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
        "{\"status\":401,\"code\":\"unauthorized\",\"message\":\"missing or invalid token\"}");
}

static enum MHD_Result invalid_id(struct MHD_Connection *conn) {
    return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
        "{\"status\":400,\"code\":\"validation_error\",\"message\":\"invalid sub_account_id\"}");
}

// Parse "/{id}" → returns true and sets *out on a valid positive integer.
static bool parse_sub_account_id(const char *subpath, AccountId *out) {
    if (!subpath || subpath[0] != '/' || subpath[1] == '\0') return false;
    char *endptr = NULL;
    long v = strtol(subpath + 1, &endptr, 10);
    if (*endptr != '\0' || v <= 0) return false;
    out->value = (int64_t)v;
    return true;
}

static json_t *affiliate_view_to_json(const AffiliateView *v) {
    json_t *o = json_object();
    json_object_set_new(o, "recipient_sub_account_id", json_integer((json_int_t)v->recipient_sub_account_id));
    json_object_set_new(o, "nickname",                 json_string(v->nickname));
    json_object_set_new(o, "recipient_full_name",      json_string(v->recipient_full_name));
    json_object_set_new(o, "currency",                 json_string(v->currency));
    return o;
}

// ── GET /api/affiliates ───────────────────────────────────────────────────

static enum MHD_Result handle_list(AppState *state, struct MHD_Connection *conn) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) return auth_401(conn);

    const char *page_s      = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "page");
    const char *page_size_s = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "page_size");

    ListAffiliatesParams params;
    memset(&params, 0, sizeof params);
    if (page_s)      { params.has_page = true;      params.page = (uint32_t)strtoul(page_s, NULL, 10); }
    if (page_size_s) { params.has_page_size = true; params.page_size = (uint32_t)strtoul(page_size_s, NULL, 10); }
    params.search_opt            = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "search");
    params.currency_opt          = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "currency");
    params.for_send_currency_opt = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "for_send_currency");
    params.sort_opt              = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "sort");

    PaginatedAffiliatesView view;
    if (!affiliate_service_list_affiliates_view(
            state->affiliate_svc, claims.sub, &params, &view, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *items = json_array();
    for (size_t i = 0; i < view.count; ++i)
        json_array_append_new(items, affiliate_view_to_json(&view.items[i]));

    json_t *root = json_object();
    json_object_set_new(root, "items",     items);
    json_object_set_new(root, "page",      json_integer((json_int_t)view.page));
    json_object_set_new(root, "page_size", json_integer((json_int_t)view.page_size));
    json_object_set_new(root, "total",     json_integer((json_int_t)view.total));

    paginated_affiliates_view_free(&view);

    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

// ── GET /api/affiliates/{sub_account_id} ──────────────────────────────────

static enum MHD_Result handle_get(AppState *state, struct MHD_Connection *conn,
                                  AccountId sub_account_id) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) return auth_401(conn);

    AffiliateView view;
    if (!affiliate_service_get_affiliate_view(
            state->affiliate_svc, claims.sub, sub_account_id, &view, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *o = affiliate_view_to_json(&view);
    char *s = json_dumps(o, JSON_COMPACT);
    json_decref(o);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

// ── POST /api/affiliates ──────────────────────────────────────────

static enum MHD_Result handle_create(AppState *state, struct MHD_Connection *conn,
                                     AuthClaims claims, json_t *root) {
    json_t *sub_j       = json_object_get(root, "recipient_sub_account_id");
    const char *nickname = json_string_value(json_object_get(root, "nickname"));
    if (!json_is_integer(sub_j) || !nickname) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"validation_error\","
            "\"message\":\"recipient_sub_account_id and nickname are required\"}");
    }
    AccountId sub = { (int64_t)json_integer_value(sub_j) };
    ServiceError serr;
    if (!affiliate_service_create_affiliate(
            state->affiliate_svc, claims.sub, sub, nickname, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return http_send_empty(conn, MHD_HTTP_OK);
}

// ── PATCH /api/affiliates/{sub_account_id} ──────────────────────────────

static enum MHD_Result handle_rename(AppState *state, struct MHD_Connection *conn,
                                     AuthClaims claims, AccountId sub, json_t *root) {
    const char *nickname = json_string_value(json_object_get(root, "nickname"));
    if (!nickname) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"validation_error\",\"message\":\"nickname is required\"}");
    }
    ServiceError serr;
    if (!affiliate_service_rename_affiliate(
            state->affiliate_svc, claims.sub, sub, nickname, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return http_send_empty(conn, MHD_HTTP_OK);
}

// ── POST /api/affiliates/resolve-target ───────────────────────────────────

static enum MHD_Result handle_resolve_target(AppState *state, struct MHD_Connection *conn,
                                             AuthClaims claims, json_t *root) {
    const char *identifier_type = json_string_value(json_object_get(root, "identifier_type"));
    const char *identifier      = json_string_value(json_object_get(root, "identifier"));
    if (!identifier_type || !identifier) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"validation_error\","
            "\"message\":\"identifier_type and identifier are required\"}");
    }

    if (strcmp(identifier_type, "phone") == 0) {
        ServiceError e = service_error_validation("phone identifier not supported yet");
        return http_send_service_error(conn, &e);
    }
    if (strcmp(identifier_type, "tag") != 0) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"validation_error\","
            "\"message\":\"invalid identifier_type\"}");
    }

    ResolvedAffiliateTargetView view;
    ServiceError serr;
    if (!affiliate_service_resolve_target_by_tag(
            state->affiliate_svc, claims.sub, identifier, &view, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *currencies = json_array();
    for (size_t i = 0; i < view.currency_count; ++i) {
        json_t *co = json_object();
        json_object_set_new(co, "currency", json_string(view.currencies[i].currency));
        json_object_set_new(co, "recipient_sub_account_id",
            json_integer((json_int_t)view.currencies[i].recipient_sub_account_id));
        json_array_append_new(currencies, co);
    }

    json_t *root_out = json_object();
    json_object_set_new(root_out, "recipient_user_id",   json_integer((json_int_t)view.recipient_user_id));
    json_object_set_new(root_out, "recipient_full_name", json_string(view.recipient_full_name));
    json_object_set_new(root_out, "currencies",          currencies);

    resolved_affiliate_target_view_free(&view);

    char *s = json_dumps(root_out, JSON_COMPACT);
    json_decref(root_out);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

// ── DELETE /api/affiliates/{sub_account_id} ───────────────────────────────

static enum MHD_Result handle_delete(AppState *state, struct MHD_Connection *conn,
                                     AccountId sub) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) return auth_401(conn);

    if (!affiliate_service_delete_affiliate(
            state->affiliate_svc, claims.sub, sub, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return http_send_empty(conn, MHD_HTTP_OK);
}

// Dispatch a fully-buffered POST/PATCH body.
static enum MHD_Result dispatch_body(AppState *state, struct MHD_Connection *conn,
                                     const char *subpath, const char *method,
                                     const char *body) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) return auth_401(conn);

    json_error_t jerr;
    json_t *root = json_loads(body, 0, &jerr);
    if (!root) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"bad_request\",\"message\":\"invalid JSON body\"}");
    }

    enum MHD_Result result;
    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
        if (!subpath || subpath[0] == '\0' || strcmp(subpath, "/") == 0) {
            result = handle_create(state, conn, claims, root);
        } else if (strcmp(subpath, "/resolve-target") == 0) {
            result = handle_resolve_target(state, conn, claims, root);
        } else {
            result = http_send_not_found(conn);
        }
    } else {
        AccountId sub;
        if (parse_sub_account_id(subpath, &sub)) {
            result = handle_rename(state, conn, claims, sub, root);
        } else {
            result = invalid_id(conn);
        }
    }

    json_decref(root);
    return result;
}

// ── Dispatcher ──────────────────────────────────────────────────────────

enum MHD_Result http_affiliates_dispatch(
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
        if (!subpath || subpath[0] == '\0' || strcmp(subpath, "/") == 0) {
            return handle_list(state, conn);
        }
        AccountId sub;
        if (parse_sub_account_id(subpath, &sub)) {
            return handle_get(state, conn, sub);
        }
        return invalid_id(conn);
    }

    if (strcmp(method, MHD_HTTP_METHOD_DELETE) == 0) {
        AccountId sub;
        if (parse_sub_account_id(subpath, &sub)) {
            return handle_delete(state, conn, sub);
        }
        return invalid_id(conn);
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0 ||
        strcmp(method, MHD_HTTP_METHOD_PATCH) == 0) {
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
        enum MHD_Result result = dispatch_body(state, conn, subpath ? subpath : "", method, body);

        body_buffer_free(bb);
        *con_cls = NULL;
        return result;
    }

    return http_send_not_found(conn);
}
