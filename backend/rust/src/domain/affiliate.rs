use crate::domain::errors::DomainError;
use crate::domain::ids::{AccountId, UserId};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Affiliate {
    owner_user_id: UserId,
    recipient_sub_account_id: AccountId,
    nickname: String
}

impl Affiliate {

    pub fn new(
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
        nickname: impl Into<String>,
    ) -> Result<Self, DomainError> {
        // 1. Trim and validate nickname
        let nickname = normalize_required(nickname.into(), "nickname")?;

        Ok(Self {
            owner_user_id,
            recipient_sub_account_id,
            nickname
        })

    }

    pub fn owner_user_id(&self) -> UserId {
        self.owner_user_id
    }

    pub fn recipient_sub_account_id(&self) -> AccountId {
        self.recipient_sub_account_id
    }

    pub fn nickname(&self) -> &str {
        &self.nickname
    }
}

fn normalize_required(s: String, field: &str) -> Result<String, DomainError> {
    // 1. Trim whitespace; reject if nothing remains
    let v = s.trim().to_string();
    if v.is_empty() {
        return Err(DomainError::validation(format!("{field} must not be empty")));
    }
    Ok(v)
}