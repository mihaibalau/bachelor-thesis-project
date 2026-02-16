//! Email value object.
//! Keeps validation in one place and guarantees: if you have an `Email`, it's valid.

use core::fmt;
use core::str::FromStr;

use crate::domain::errors::DomainError;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Email(String);

impl Email {
    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn into_string(self) -> String {
        self.0
    }
}

impl fmt::Display for Email {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl FromStr for Email {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let s = s.trim();
        if s.is_empty() {
            return Err(DomainError::validation("Email must not be empty"));
        }

        let parts: Vec<&str> = s.split('@').collect();
        if parts.len() != 2 {
            return Err(DomainError::validation("Email must contain exactly one '@'"));
        }

        let local = parts[0];
        let domain = parts[1];

        if local.is_empty() || domain.is_empty() {
            return Err(DomainError::validation("Email local/domain must not be empty"));
        }
        if !domain.contains('.') {
            return Err(DomainError::validation("Email domain must contain a '.'"));
        }

        Ok(Email(s.to_string()))
    }
}

impl TryFrom<String> for Email {
    type Error = DomainError;

    fn try_from(value: String) -> Result<Self, Self::Error> {
        Email::from_str(&value)
    }
}

impl From<Email> for String {
    fn from(value: Email) -> Self {
        value.0
    }
}
