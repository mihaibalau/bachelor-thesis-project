//! IBAN value object.
//! Keeps validation in one place and guarantees: if you have an `IBAN`, it's valid.

use core::fmt;
use core::str::FromStr;

use crate::domain::errors::DomainError;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct IBAN(String);

impl IBAN {
    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn into_string(self) -> String {
        self.0
    }
}

impl fmt::Display for IBAN {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl FromStr for IBAN {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let raw = s.trim();
        if raw.is_empty() {
            return Err(DomainError::validation("IBAN must not be empty"));
        }

        let iban: String = raw.chars().filter(|c| !c.is_whitespace()).collect::<String>().to_ascii_uppercase();

        if iban.len() != 24 { return Err(DomainError::validation("Invalid IBAN length")); }
        if !iban.starts_with("RO") { return Err(DomainError::validation("Invalid country code")); }

        Ok(IBAN(iban))
    }
}

impl TryFrom<String> for IBAN {
    type Error = DomainError;

    fn try_from(value: String) -> Result<Self, Self::Error> {
        IBAN::from_str(&value)
    }
}

impl From<IBAN> for String {
    fn from(value: IBAN) -> Self {
        value.0
    }
}
