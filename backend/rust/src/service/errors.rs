use anyhow::Error as AnyError;
use thiserror::Error;

use crate::db::errors::RepoError;
use crate::domain::errors::DomainError;

pub type ServiceResult<T> = Result<T, ServiceError>;

// Service-layer errors (maps to HTTP in server/error.rs)
#[derive(Debug, Error)]
pub enum ServiceError {

    #[error("{entity} not found")]
    NotFound {
        entity: &'static str,
    },

    #[error("conflict on {entity}: {message}")]
    Conflict {
        entity: &'static str,
        message: String,
    },

    #[error("validation error: {0}")]
    Validation(String),

    #[error(transparent)]
    Domain(#[from] DomainError),

    #[error(transparent)]
    Repo(#[from] RepoError),

    #[error("concurrency error: {0}")]
    Concurrency(String),

    #[error("forbidden")]
    Forbidden,

    #[error("unexpected error: {0}")]
    Unexpected(#[from] AnyError),
}

impl ServiceError {
    pub fn not_found(entity: &'static str) -> Self{
        ServiceError::NotFound { entity }
    }

    pub fn conflict(entity: &'static str, msg: impl Into<String>) -> Self {
        ServiceError::Conflict {
            entity,
            message: msg.into(),
        }
    }

    pub fn internal(msg: impl Into<String>) -> Self {
        ServiceError::Unexpected(AnyError::msg(msg.into()))
    }

    pub fn with_context(self, ctx: impl Into<String>) -> ServiceError {
        ServiceError::Unexpected(AnyError::new(self).context(ctx.into()))
    }
}
