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

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct AccountId(pub i64);

impl fmt::Display for AccountId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<i64> for AccountId {
    fn from(value: i64) -> Self {
        Self(value)
    }
}

impl From<AccountId> for i64{
    fn from(value: AccountId) -> Self{
        value.0
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct TransactionId(pub i64);

impl fmt::Display for TransactionId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl From<i64> for TransactionId {
    fn from(value: i64) -> Self {
        Self(value)
    }
}

impl From<TransactionId> for i64{
    fn from(value: TransactionId) -> Self{
        value.0
    }
}
