use std::sync::Arc;
use async_trait::async_trait;
use crate::db::account_repo::AccountRepo;
use crate::db::errors::RepoError;
use crate::domain::account::Account;
use crate::domain::ids::{AccountId, UserId};
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;
use crate::service::errors::{ServiceError, ServiceResult};

/// Repository abstraction for accounts
///
/// Defining this as a trait lets you unit-test `AccountService` with
/// in-memory fakes instead of hitting Postgres
#[async_trait]
pub trait AccountRepository: Send + Sync {
    async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>;
    async fn exists_by_account_type(
        &self,
        user_id: UserId,
        account_type: AccountType,
    ) -> Result<bool, RepoError>;
    async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>;
    async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>;
    async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>;
}

/// Thin adapter: your concrete SQLx repo implements the trait
///
/// This mirrors the "port/adapter" pattern and keeps the service layer
/// decoupled from SQLx's exact API
#[async_trait]
impl AccountRepository for AccountRepo {
    async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError> {
        self.list_for_user(user_id).await
    }

    async fn exists_by_account_type(
        &self,
        user_id: UserId,
        account_type: AccountType,
    ) -> Result<bool, RepoError> {
        self.exists_by_account_type(user_id, account_type).await
    }

    async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError> {
        self.exists_by_iban(iban).await
    }

    async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError> {
        self.get_by_id(account_id).await
    }

    async fn insert(&self, account: &Account) -> Result<AccountId, RepoError> {
        self.insert(account).await
    }
}

/// Service responsible for account lifecycle and balance-heavy operations
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
        initial_balance_cents: i64
    ) -> ServiceResult<AccountId> {

        // 1. Enforce at-most-one account of a given type for this user
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

        // 2. Generate unique IBAN for the new account
        let mut iban = IBAN::generate()?;
        while self.repo.exists_by_iban(iban.as_str()).await? {
            let iban = IBAN::generate()?;
        }

        // 3. Build the domain `Account`
        // Following the same "rehydrate vs constructors" pattern as your repos
        // (see TryFrom<AccountRow>), here we use a dedicated constructor for
        // new accounts
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

    /// Load a single account, mapping RepoError::NotFound into ServiceError::NotFound
    pub async fn get_account(&self, account_id: AccountId) -> ServiceResult<Account> {
        match self.repo.get_by_id(account_id).await {
            Ok(account) => Ok(account),
            Err(RepoError::NotFound(_)) => Err(ServiceError::not_found("account")),
            Err(e) => Err(ServiceError::from(e))
         }
    }
}