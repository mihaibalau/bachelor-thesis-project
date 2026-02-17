use core::fmt;
use core::str::FromStr;

use crate::domain::errors::DomainError;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum Currency {
    Ron,
    Eur,
    Usd,
}

impl Currency {
    pub fn as_str(&self) -> &str {
        match self {
            Currency::Ron => "RON",
            Currency::Eur => "EUR",
            Currency::Usd => "USD",
        }
    }
}

impl fmt::Display for Currency {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl FromStr for Currency {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let s = s.trim().to_ascii_uppercase();
        match s.as_str() {
            "RON" => Ok(Currency::Ron),
            "EUR" => Ok(Currency::Eur),
            "USD" => Ok(Currency::Usd),
            _ => Err(DomainError::validation("Invalid currency type")),
        }
    }
}

impl From<Currency> for String {
    fn from(value: Currency) -> Self {
        value.as_str().to_string()
    }
}
