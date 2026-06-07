#include "include/http_error.h"

ApiErrorBody http_error_from_service_error(const ServiceError *err) {
    ApiErrorBody body;

    if (!err || err->code == SERVICE_ERROR_NONE) {
        body.code    = "ok";
        body.message = "";
        body.status  = 200;
        return body;
    }

    // Map ServiceError to HTTP status + code (parity with Rust server/error.rs).
    switch (err->code) {
        case SERVICE_ERROR_NOT_FOUND:
            body.status  = 404;
            body.code    = "not_found";
            break;
        case SERVICE_ERROR_CONFLICT:
            body.status  = 409;
            body.code    = "conflict";
            break;
        case SERVICE_ERROR_VALIDATION:
            body.status  = 400;
            body.code    = "validation_error";
            break;
        case SERVICE_ERROR_DOMAIN:
            body.status  = 400;
            body.code    = "domain_error";
            break;
        case SERVICE_ERROR_REPO:
            body.status  = 500;
            body.code    = "repo_error";
            break;
        case SERVICE_ERROR_CONCURRENCY:
            body.status  = 409;
            body.code    = "concurrency_error";
            break;
        case SERVICE_ERROR_FORBIDDEN:
            body.status  = 403;
            body.code    = "forbidden";
            break;
        case SERVICE_ERROR_UNEXPECTED:
        default:
            body.status  = 500;
            body.code    = "unexpected_error";
            break;
    }

    body.message = err->message[0] ? err->message : "";
    return body;
}