use core::fmt;
use std::str::FromStr;
use crate::domain::errors::DomainError;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TransactionType {
    Deposit,
    Withdrawal,
    Send,
    Transfer
}

impl TransactionType {
    pub fn as_str(&self) -> &str {
        match self {
            TransactionType::Deposit => "deposit",
            TransactionType::Withdrawal => "withdrawal",
            TransactionType::Send => "send",
            TransactionType::Transfer => "transfer",
        }
    }
}

impl fmt::Display for TransactionType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl FromStr for TransactionType {
    type Err = DomainError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let s = s.trim();
        if s.eq_ignore_ascii_case("Deposit") { return Ok(TransactionType::Deposit); }
        if s.eq_ignore_ascii_case("Withdrawal")  { return Ok(TransactionType::Withdrawal); }
        if s.eq_ignore_ascii_case("Send") { return Ok(TransactionType::Send); }
        if s.eq_ignore_ascii_case("Transfer") { return Ok(TransactionType::Transfer); }
        Err(DomainError::validation("Invalid transaction type"))
    }

}

impl From<TransactionType> for String {
    fn from(value: TransactionType) -> Self {
        value.as_str().to_string()
    }
}
