use std::sync::Arc;
use crate::db::errors::RepoError;
use crate::domain::account::Account;
use crate::domain::ids::{AccountId, UserId};
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;
use crate::service::errors::{ServiceError, ServiceResult};
use crate::service::user::AccountRepository;

/// Service responsible for account lifecycle and balance-heavy operations.
#[derive(Clone)]
pub struct AccountService<R>
where
    R: AccountRepository,
{
    repo: Arc<R>,
}

impl<R> AccountService<R>
where
    R: AccountRepository,
{
    pub fn new(repo: Arc<R>) -> Self {
        Self { repo }
    }

    pub async fn open_account(
        &self,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
        initial_balance_cents: i64,
        iban_str: String
    ) -> ServiceResult<AccountId> {
        // 1. Parse IBAN using the domain type.
        let iban: IBAN = iban_str.parse().map_err(ServiceError::Domain)?;

        // 2. Enforce at-most-one account of a given type for this user.
        if self.repo.exists_by_account_type(user_id, account_type).await? {
            return Err(ServiceError::conflict(
                "account",
                format!(
                    "user {} already has an account type of {}",
                    user_id.0,
                    account_type.as_str()
                ),
            ));
        }

        // 3. Enforce global IBAN uniqueness.
        if self.repo.exists_by_iban(iban.as_str()).await? {
            return Err(ServiceError::conflict(
                "account",
                format!("IBAN '{}' is already in use", iban.as_str()),
            ));
        }

        // 4. Build the domain `Account`.
        // Following the same "rehydrate vs constructors" pattern as your repos
        // (see TryFrom<AccountRow>), here we use a dedicated constructor for
        // new accounts.
        let account = Account::create(
            user_id,
            account_type,
            currency,
            initial_balance_cents,
            iban,
        )?;

        let account_id = self.repo.insert(&account).await?;
        Ok(account_id)
    }

    /// Load a single account, mapping RepoError::NotFound into ServiceError::NotFound.
    pub async fn get_account(&self, account_id: AccountId) -> ServiceResult<Account> {
        match self.repo.get_by_id(account_id).await {
            Ok(account) => Ok(account),
            Err(RepoError::NotFound(_)) => Err(ServiceError::not_found("account")),
            Err(e) => Err(ServiceError::from(e))
         }
    }
}