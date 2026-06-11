use crate::domain::ids::{AccountId, TransactionId};
use chrono::{DateTime, Utc};
use crate::domain::errors::DomainError;
use crate::domain::value::transaction_type::TransactionType;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Transaction {
    id: Option<TransactionId>,

    from_account_id: AccountId,
    to_account_id: AccountId,

    transaction_type: TransactionType,
    value_cents: i64,

    recorded_on: DateTime<Utc>,
    description: String
}

impl Transaction {
    pub fn create(
        from_account_id: AccountId,
        to_account_id: AccountId,
        transaction_type: TransactionType,
        value_cents: i64,
        description: String
    ) -> Result<Self, DomainError> {
        Self::build(
            None,
            from_account_id,
            to_account_id,
            transaction_type,
            value_cents,
            None,
            description
        )
    }

    pub fn rehydrate(
        id: TransactionId,
        from_account_id: AccountId,
        to_account_id: AccountId,
        transaction_type: TransactionType,
        value_cents: i64,
        recorded_on: DateTime<Utc>,
        description: String
    ) -> Result<Self, DomainError> {
        Self::build(
            Some(id),
            from_account_id,
            to_account_id,
            transaction_type,
            value_cents,
            Some(recorded_on),
            description
        )
    }

    fn build(
        id: Option<TransactionId>,
        from_account_id: AccountId,
        to_account_id: AccountId,
        transaction_type: TransactionType,
        value_cents: i64,
        recorded_on: Option<DateTime<Utc>>,
        description: String
    ) -> Result<Self, DomainError> {
        // 1. Reject negative amounts and self-transfers
        if value_cents < 0 {
            return Err(DomainError::validation("Value must be >= 0"));
        }

        if from_account_id == to_account_id {
            return Err(DomainError::validation(
                "From Account ID must be different from To Account ID",
            ));
        }

        // 2. Normalize description and default recorded_on for new transactions
        let description = normalize_required(description.into(), "Description")?;

        Ok(Self {
            id,
            from_account_id,
            to_account_id,
            transaction_type,
            value_cents,
            recorded_on: recorded_on.unwrap_or_else(|| Utc::now()),
            description
        })
    }

    pub fn id(&self) -> Option<TransactionId> {
        self.id
    }

    pub fn from_account_id(&self) -> AccountId {
        self.from_account_id
    }

    pub fn to_account_id(&self) -> AccountId {
        self.to_account_id
    }

    pub fn transaction_type(&self) -> &TransactionType {
        &self.transaction_type
    }

    pub fn transaction_type_str(&self) -> &str {
        &self.transaction_type.as_str()
    }

    pub fn value_cents(&self) -> i64 {
        self.value_cents
    }

    pub fn recorded_on(&self) -> DateTime<Utc> {
        self.recorded_on
    }

    pub fn description(&self) -> &str {
        &self.description
    }

    pub(crate) fn set_id_after_insert(&mut self, id: TransactionId) {
        self.id = Some(id);
    }
}


fn normalize_required(s: String, field: &str) -> Result<String, DomainError> {
    // Trim whitespace; reject if nothing remains
    let v = s.trim().to_string();
    if v.is_empty() {
        return Err(DomainError::validation(format!("{field} must not be empty")));
    }
    Ok(v)
}