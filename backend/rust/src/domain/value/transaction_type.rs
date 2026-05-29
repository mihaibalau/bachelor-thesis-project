use core::fmt;
use core::str::FromStr;
use crate::domain::errors::DomainError;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum TransactionType {
    Deposit,
    Withdrawal,
    Send,
    Transfer,
    Payment
}

impl TransactionType {
    pub fn as_str(&self) -> &str {
        match self {
            TransactionType::Deposit    => "Deposit",
            TransactionType::Withdrawal => "Withdrawal",
            TransactionType::Send       => "Send",
            TransactionType::Transfer   => "Transfer",
            TransactionType::Payment    => "Payment",
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
        match s {
            "Deposit"    => Ok(TransactionType::Deposit),
            "Withdrawal" => Ok(TransactionType::Withdrawal),
            "Send"       => Ok(TransactionType::Send),
            "Transfer"   => Ok(TransactionType::Transfer),
            "Payment"    => Ok(TransactionType::Payment),
            other => Err(DomainError::validation(format!("invalid TransactionType: {other}"))),
        }
    }
}

impl From<TransactionType> for String {
    fn from(value: TransactionType) -> Self {
        value.as_str().to_string()
    }
}
