
use crate::domain::errors::DomainError;
use crate::domain::ids::{AccountId, UserId};
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;

#[derive(Debug, Clone, PartialEq)]
pub struct Account{
    id: Option<AccountId>,
    user_id: UserId,
    account_type: AccountType,
    currency: Currency,
    balance_cents: i64,
    iban: IBAN
}

impl Account{
    pub fn create(
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
        balance_cents: i64,
        iban: IBAN
    ) -> Result<Self, DomainError> {
        Self::build(
            None,
            user_id,
            account_type,
            currency,
            balance_cents,
            iban
        )
    }

    pub fn rehydrate(
        id: AccountId,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
        balance_cents: i64,
        iban: IBAN
    ) -> Result<Self, DomainError> {
        Self::build(
            Some(id),
            user_id,
            account_type,
            currency,
            balance_cents,
            iban
        )
    }

    fn build(
        id: Option<AccountId>,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
        balance_cents: i64,
        iban: IBAN,
    ) -> Result<Self, DomainError> {
        // 1. Reject negative opening balance
        if balance_cents < 0 {
            return Err(DomainError::validation("Balance must be >= 0"));
        }

        Ok(Self {
            id,
            user_id,
            account_type,
            currency,
            balance_cents,
            iban,
        })
    }

    pub fn id(&self) -> Option<AccountId> { self.id }
    pub fn user_id(&self) -> UserId { self.user_id }
    pub fn account_type(&self) -> AccountType { self.account_type }
    pub fn currency(&self) -> Currency { self.currency }
    pub fn balance_cents(&self) -> i64 { self.balance_cents }
    pub fn iban(&self) -> &IBAN { &self.iban }

    pub fn credit(&mut self, amount_cents: i64) -> Result<(), DomainError> {
        // 1. Require a positive credit amount
        if amount_cents <= 0 {
            return Err(DomainError::validation("Credit amount must be > 0"));
        }
        // 2. Add to balance, guarding against i64 overflow
        self.balance_cents = self
            .balance_cents
            .checked_add(amount_cents)
            .ok_or_else(|| DomainError::validation("Balance overflow"))?;
        Ok(())
    }

    pub fn debit(&mut self, amount_cents: i64) -> Result<(), DomainError> {
        // 1. Require a positive debit amount
        if amount_cents <= 0 {
            return Err(DomainError::validation("Debit amount must be > 0"));
        }
        // 2. Ensure sufficient funds before subtracting
        if self.balance_cents < amount_cents {
            return Err(DomainError::validation("Insufficient funds"));
        }
        self.balance_cents -= amount_cents;
        Ok(())
    }

    pub(crate) fn set_id_after_insert(&mut self, id: AccountId) {
        self.id = Some(id);
    }
}