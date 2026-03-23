use anyhow::Error as AnyError;
use thiserror::Error;

use crate::db::errors::RepoError;
use crate::domain::errors::DomainError;

pub type ServiceResult<T> = Result<T, ServiceError>;

/// Inspired by the error layering approach in *Zero To Production In Rust*,
/// chapter 8 ("Error Handling").
#[derive(Debug, Error)]
pub enum ServiceError {

    /// Entity was not found in persistent storage.
    #[error("{entity} not found")]
    NotFound {
        entity: &'static str,
    },

    /// Business-level conflict: invariants violated but not because of
    /// a low-level database failure.
    #[error("conflict on {entity}: {message}")]
    Conflict {
        entity: &'static str,
        message: String,
    },

    /// Validation error at the application/service layer.
    #[error("validation error: {0}")]
    Validation(String),

    /// Rich domain errors surfaced directly to the service.
    #[error(transparent)]
    Domain(#[from] DomainError),

    /// Repository / persistence error. Wraps `RepoError`, which in turn
    /// may wrap SQLx errors or domain mapping failures.
    #[error(transparent)]
    Repo(#[from] RepoError),

    /// Concurrency / coordination errors at the service layer.
    #[error("concurrency error: {0}")]
    Concurrency(String),

    /// Catch-all for truly unexpected situations where we want
    /// to bubble up a rich error chain (e.g. via `anyhow`).
    ///
    /// see Zero To Production, section 8.4.
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

    /// Attach extra human-readable context while preserving the error chain.
    ///
    /// Mirroring the `context` / `with_context` pattern used with `anyhow`
    /// in Zero To Production, chapter 9.
    pub fn with_context(self, ctx: impl Into<String>) -> ServiceError {
        ServiceError::Unexpected(AnyError::new(self).context(ctx.into()))
    }
}