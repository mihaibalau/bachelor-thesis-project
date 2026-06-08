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
        // Case-insensitive parse normalized to the canonical Title-Case the DB
        // CHECK constraint requires (matches the C strcasecmp behaviour).
        match s.to_ascii_lowercase().as_str() {
            "deposit"    => Ok(TransactionType::Deposit),
            "withdrawal" => Ok(TransactionType::Withdrawal),
            "send"       => Ok(TransactionType::Send),
            "transfer"   => Ok(TransactionType::Transfer),
            "payment"    => Ok(TransactionType::Payment),
            _ => Err(DomainError::validation(format!("invalid TransactionType: {s}"))),
        }
    }
}

impl From<TransactionType> for String {
    fn from(value: TransactionType) -> Self {
        value.as_str().to_string()
    }
}
