
use core::fmt;

use crate::domain::errors::DomainError;

#[derive(Debug)]
pub enum RepoError {
    Db(sqlx::Error),
    NotFound(&'static str),
    Domain(DomainError),
}

impl RepoError {
    pub fn not_found(entity: &'static str) -> Self {
        RepoError::NotFound(entity)
    }
}

impl fmt::Display for RepoError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            RepoError::Db(e) => write!(f, "db error: {e}"),
            RepoError::NotFound(entity) => write!(f, "{entity} not found"),
            RepoError::Domain(e) => write!(f, "domain error: {e}"),
        }
    }
}

impl std::error::Error for RepoError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        // NotFound is a leaf error with no underlying cause
        match self {
            RepoError::Db(e) => Some(e),
            RepoError::Domain(e) => Some(e),
            RepoError::NotFound(_) => None,
        }
    }
}

impl From<sqlx::Error> for RepoError {
    fn from(value: sqlx::Error) -> Self {
        RepoError::Db(value)
    }
}

impl From<DomainError> for RepoError {
    fn from(value: DomainError) -> Self {
        RepoError::Domain(value)
    }
}

pub type RepoResult<T> = Result<T, RepoError>;
