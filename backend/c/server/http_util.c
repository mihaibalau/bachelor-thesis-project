#include "include/http_util.h"
#include "include/http_error.h"
#include "../util/include/log.h"

#include <jansson.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HTTP_BODY_MAX_BYTES (1024 * 1024)

/*
 * libmicrohttpd delivers POST bodies in chunks across callbacks.
 * con_cls holds a BodyBuffer* between calls until the full body is read.
 */

// ── Request body buffering ────────────────────────────────────────────────

BodyBuffer *body_buffer_new(void) {
    return calloc(1, sizeof(BodyBuffer));
}

void body_buffer_free(BodyBuffer *bb) {
    if (!bb) return;
    free(bb->buf);
    free(bb);
}

bool body_buffer_append(BodyBuffer *bb, const char *data, size_t size) {
    // 1. Enforce max body size.
    if (bb->len + size + 1 > HTTP_BODY_MAX_BYTES) return false;
    // 2. Grow buffer if needed.
    if (bb->len + size + 1 > bb->cap) {
        size_t new_cap = bb->cap ? bb->cap * 2 : 4096;
        while (new_cap < bb->len + size + 1) new_cap *= 2;
        char *tmp = realloc(bb->buf, new_cap);
        if (!tmp) return false;
        bb->buf = tmp;
        bb->cap = new_cap;
    }
    memcpy(bb->buf + bb->len, data, size);
    bb->len += size;
    bb->buf[bb->len] = '\0';
    return true;
}

// ── Response helpers ──────────────────────────────────────────────────────

void add_cors_headers(struct MHD_Response *res) {
    const char *origin = getenv("CORS_ORIGIN");
    if (!origin || !origin[0]) origin = "http://localhost:5173";
    MHD_add_response_header(res, "Access-Control-Allow-Origin", origin);
    MHD_add_response_header(res, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    MHD_add_response_header(res, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    MHD_add_response_header(res, "Access-Control-Max-Age", "86400");
}

enum MHD_Result http_send_json(
    struct MHD_Connection *conn,
    int status_code,
    const char *json
) {
    struct MHD_Response *res = MHD_create_response_from_buffer(
        strlen(json), (void *)json, MHD_RESPMEM_MUST_COPY);

    if (!res) {
        LOG_ERROR("http_response", "failed to allocate JSON response status=%d", status_code);
        return MHD_NO;
    }

    MHD_add_response_header(res, "Content-Type", "application/json");
    add_cors_headers(res);

    enum MHD_Result ret = MHD_queue_response(conn, (unsigned int)status_code, res);
    MHD_destroy_response(res);
    return ret;
}

enum MHD_Result http_send_empty(
    struct MHD_Connection *conn,
    int status_code
) {
    struct MHD_Response *res = MHD_create_response_from_buffer(
        0, (void *)"", MHD_RESPMEM_PERSISTENT);

    if (!res) {
        LOG_ERROR("http_response", "failed to allocate empty response status=%d", status_code);
        return MHD_NO;
    }

    add_cors_headers(res);

    enum MHD_Result ret = MHD_queue_response(conn, (unsigned int)status_code, res);
    MHD_destroy_response(res);
    return ret;
}

enum MHD_Result http_send_service_error(
    struct MHD_Connection *conn,
    const ServiceError *err
) {
    ApiErrorBody body = http_error_from_service_error(err);

    if (body.status >= 500) {
        LOG_ERROR("api_error", "status=%d code=%s message=%s",
            body.status, body.code, body.message ? body.message : "");
    } else if (body.status >= 400) {
        LOG_WARN("api_error", "status=%d code=%s message=%s",
            body.status, body.code, body.message ? body.message : "");
    } else if (log_is_debug()) {
        LOG_DEBUG("api_error", "status=%d code=%s message=%s",
            body.status, body.code, body.message ? body.message : "");
    }

    // Build ErrorBody JSON (matches Rust shape).
    json_t *root = json_object();
    json_object_set_new(root, "status",  json_integer(body.status));
    json_object_set_new(root, "code",    json_string(body.code));
    json_object_set_new(root, "message", json_string(body.message ? body.message : ""));

    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (json_str) {
        ret = http_send_json(conn, body.status, json_str);
        free(json_str);
    } else {
        LOG_ERROR("api_error", "json serialization failed for service error");
        ret = http_send_json(conn, 500,
            "{\"status\":500,\"code\":\"unexpected_error\","
            "\"message\":\"json serialization failed\"}");
    }
    return ret;
}

enum MHD_Result http_send_not_found(struct MHD_Connection *conn) {
    LOG_DEBUG("http_not_found", "resource not found");
    return http_send_json(conn, MHD_HTTP_NOT_FOUND,
        "{\"status\":404,\"code\":\"not_found\",\"message\":\"resource not found\"}");
}
