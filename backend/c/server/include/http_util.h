#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H

#include <microhttpd.h>
#include <stdbool.h>
#include <stddef.h>

#include "service_error.h"

/*
 * Shared HTTP helpers used by every route file (http_users, http_accounts,
 * http_affiliates, http_transactions).
 *
 * Keeping these in one place mirrors the way the Rust backend centralises
 * response building (axum's IntoResponse + the shared ApiError) instead of
 * re-implementing it per route module.
 */

/* Request body buffering (libmicrohttpd POST/PATCH pattern)
 * libmicrohttpd calls the handler multiple times for the same request:
 *   1. *con_cls == NULL        → allocate the buffer
 *   2. *upload_data_size > 0   → receiving data
 *   3. *upload_data_size == 0  → body complete, process it
 *
 * Equivalent of Rust's `Json(body): Json<T>` extractor.
 */
typedef struct BodyBuffer {
    char  *buf;
    size_t len;
    size_t cap;
} BodyBuffer;

BodyBuffer *body_buffer_new(void);
void        body_buffer_free(BodyBuffer *bb);
bool        body_buffer_append(BodyBuffer *bb, const char *data, size_t size);

// ── Response helpers ─────────────────────────────────────────────────────

void add_cors_headers(struct MHD_Response *res);

enum MHD_Result http_send_json(
    struct MHD_Connection *conn,
    int status_code,
    const char *json
);

enum MHD_Result http_send_empty(
    struct MHD_Connection *conn,
    int status_code
);


// Serialize a ServiceError into the same JSON shape as the Rust backend's
enum MHD_Result http_send_service_error(
    struct MHD_Connection *conn,
    const ServiceError *err
);

// 404 with the standard JSON body.
enum MHD_Result http_send_not_found(struct MHD_Connection *conn);

#endif /* HTTP_UTIL_H */
