use axum::{
    http::StatusCode,
    response::{IntoResponse, Response},
    Json,
};

use serde::Serialize;
use crate::service::errors::ServiceError;

#[derive(Serialize)]
struct ErrorBody {
    status: u16,
    code: &'static str,
    message: String,
}

// Wrapper for `IntoResponse` without orphan-rule issues.
pub struct ApiError(pub ServiceError);

impl From<ServiceError> for ApiError {
    fn from(err: ServiceError) -> Self {
        ApiError(err)
    }
}

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        // 1. Map ServiceError to HTTP status + body fields
        let is_forbidden = matches!(self.0, ServiceError::Forbidden);
        let (status, code, message) = match self.0 {
            ServiceError::NotFound {entity} => (
                StatusCode::NOT_FOUND,
                "not_found",
                format!("{} not found", entity)
            ),
            ServiceError::Conflict { entity, message } => (
                StatusCode::CONFLICT,
                "conflict",
                format!("{} conflict: {}", entity, message)
            ),
            ServiceError::Validation(message) => (
                StatusCode::BAD_REQUEST,
                "validation_error",
                message,
            ),
            ServiceError::Domain(e) => {
                // Emit the bare validation message (no "domain error: validation error:"
                // prefix) to match the C backend's domain-error text.
                let crate::domain::errors::DomainError::Validation(message) = e;
                (
                    StatusCode::BAD_REQUEST,
                    "domain_error",
                    message,
                )
            },
            ServiceError::Repo(e) => (
                StatusCode::INTERNAL_SERVER_ERROR,
                "repo_error",
                e.to_string(),
            ),
            ServiceError::Concurrency(msg) => (
                StatusCode::CONFLICT,
                "concurrency_error",
                msg,
            ),
            ServiceError::Forbidden => (
                StatusCode::FORBIDDEN,
                "forbidden",
                "access denied".to_string(),
            ),
            ServiceError::Unexpected(e) => (
                StatusCode::INTERNAL_SERVER_ERROR,
                "unexpected_error",
                e.to_string(),
            ),
        };

        // 2. Log client/server errors
        if status.is_server_error() {
            tracing::error!(%code, %message, "api server error");
        } else if status.is_client_error() && !is_forbidden {
            tracing::warn!(%code, %message, "api client error");
        }

        // 3. JSON error response
        (status, Json(ErrorBody { status: status.as_u16(), code, message })).into_response()
    }
}

pub type ApiResult<T> = Result<T, ApiError>;