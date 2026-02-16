
use core::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DomainError {
    Validation(String),
}

impl DomainError {
    pub fn validation(msg: impl Into<String>) -> Self {
        DomainError::Validation(msg.into())
    }
}

impl fmt::Display for DomainError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DomainError::Validation(msg) => write!(f, "validation error: {msg}"),
        }
    }
}

impl std::error::Error for DomainError {}
