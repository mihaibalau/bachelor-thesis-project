use core::fmt;
use core::str::FromStr;

use crate::domain::errors::DomainError;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum AccountType {
    Savings,
    Credit,
    Regular,
}

impl AccountType {
    pub fn as_str(&self) -> &str {
        match self {
            AccountType::Savings => "Savings",
            AccountType::Credit => "Credit",
            AccountType::Regular => "Regular",
        }
    }

    pub fn all() -> &'static [AccountType] {
        &[AccountType::Savings, AccountType::Credit, AccountType::Regular]
    }
}

impl fmt::Display for AccountType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl FromStr for AccountType {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let s = s.trim();
        if s.eq_ignore_ascii_case("Savings") { return Ok(AccountType::Savings); }
        if s.eq_ignore_ascii_case("Credit")  { return Ok(AccountType::Credit); }
        if s.eq_ignore_ascii_case("Regular") { return Ok(AccountType::Regular); }
        Err(DomainError::validation("Invalid account type"))
    }

}

impl From<AccountType> for String {
    fn from(value: AccountType) -> Self {
        value.as_str().to_string()
    }
}
