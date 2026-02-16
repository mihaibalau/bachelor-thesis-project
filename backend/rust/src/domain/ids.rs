//! Strongly-typed IDs for the domain.
//! Prevents mixing ids of different entities at compile time.

use core::fmt;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct UserId(pub i64);

impl fmt::Display for UserId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<i64> for UserId {
    fn from(value: i64) -> Self {
        Self(value)
    }
}

impl From<UserId> for i64 {
    fn from(value: UserId) -> Self {
        value.0
    }
}
