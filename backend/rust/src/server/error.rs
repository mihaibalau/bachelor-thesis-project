use axum::{
    http::StatusCode,
    response::{IntoResponse, Response},
    Json,
};

use serde::Serialize;
use crate::service::errors::ServiceError;

#[derive(Serialize)]
struct ErrorBody {
    code: &'static str,
    message: String
}

/// Small wrapper so we can implement `IntoResponse`
/// without hitting Rust's orphan rule.
pub struct ApiError(pub ServiceError);

impl From<ServiceError> for ApiError {
    fn from(err: ServiceError) -> Self {
        ApiError(err)
    }
}

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
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
                StatusCode::UNPROCESSABLE_ENTITY,
                "validation_error",
                message,
            ),
            ServiceError::Domain(e) => (
                StatusCode::BAD_REQUEST,
                "domain_error",
                format!("domain error: {}", e.to_string())
            ),
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

        (status, Json(ErrorBody { code, message })).into_response()
    }
}

/// Convenience alias for handler return type.
pub type ApiResult<T> = Result<T, ApiError>;