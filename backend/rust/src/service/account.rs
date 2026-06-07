use std::collections::{HashMap, HashSet};
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

// Repository port for accounts
#[async_trait]
pub trait AccountRepository: Send + Sync {
    async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>;
    async fn exists_by_account_type(
        &self,
        user_id: UserId,
        account_type: AccountType,
    ) -> Result<bool, RepoError>;
    async fn exists_by_type_and_currency(
        &self,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
    ) -> Result<bool, RepoError>;
    async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>;
    async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>;
    async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>;
    async fn update(&self, account: &Account) -> Result<(), RepoError>;
    async fn list_type_currency_pairs(&self, user_id: UserId) -> Result<Vec<(AccountType, Currency)>, RepoError>;
}

// SQLx adapter implementing AccountRepository
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

    async fn exists_by_type_and_currency(
        &self,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
    ) -> Result<bool, RepoError> {
        self.exists_by_type_and_currency(user_id, account_type, currency).await
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

    async fn update(&self, account: &Account) -> Result<(), RepoError> {
        self.update(account).await
    }

    async fn list_type_currency_pairs(&self, user_id: UserId) -> Result<Vec<(AccountType, Currency)>, RepoError> {
        self.list_type_currency_pairs(user_id).await
    }
}

#[derive(Clone)]
pub struct AccountService<R>
where
    R: AccountRepository,
{
    repo: Arc<R>,
}

pub struct AccountAvailability {
    pub available: HashMap<AccountType, Vec<Currency>>,
}

impl<R> AccountService<R>
where
    R: AccountRepository,
{
    pub fn new(repo: Arc<R>) -> Self {
        Self { repo }
    }

    #[tracing::instrument(skip(self), fields(user_id = %user_id.0, account_type = %account_type_str, currency = %currency_str, initial_balance_cents))]
    pub async fn open_account_raw(
        &self,
        user_id: UserId,
        account_type_str: String,
        currency_str: String,
        initial_balance_cents: i64,
    ) -> ServiceResult<AccountId> {
        // 1. Parse account_type and currency from API strings
        use core::str::FromStr;
        let account_type = AccountType::from_str(&account_type_str)
            .map_err(|_| ServiceError::Validation("invalid account_type".to_string()))?;
        let currency = Currency::from_str(&currency_str)
            .map_err(|_| ServiceError::Validation("invalid currency".to_string()))?;
        if initial_balance_cents < 0 {
            return Err(ServiceError::Validation(
                "initial_balance_cents must be >= 0".to_string(),
            ));
        }
        // 2. Delegate to typed open_account
        self.open_account(user_id, account_type, currency, initial_balance_cents).await
    }

    pub async fn open_account(
        &self,
        user_id: UserId,
        account_type: AccountType,
        currency: Currency,
        initial_balance_cents: i64
    ) -> ServiceResult<AccountId> {

        // 1. Enforce at-most-one account per (type, currency) for this user
        if self
            .repo
            .exists_by_type_and_currency(user_id, account_type, currency)
            .await?
        {
            return Err(ServiceError::conflict(
                "account",
                format!(
                    "you already have a {} {} account",
                    account_type.as_str(),
                    currency.as_str()
                ),
            ));
        }

        // 2. Insert with generated IBAN; retry on unique violation
        const MAX_RETRIES: usize = 5;
        for attempt in 0..MAX_RETRIES {
            let iban = IBAN::generate()?;

            let account = Account::create(
                user_id,
                account_type,
                currency,
                initial_balance_cents,
                iban,
            )?;

            match self.repo.insert(&account).await {
                Ok(id) => return Ok(id),
                Err(RepoError::Db(sqlx::Error::Database(db_err))) => {
                    // Postgres 23505 = unique violation (IBAN or user/type pair)
                    if db_err.code().as_deref() == Some("23505") {
                        let msg = db_err.message().to_lowercase();
                        if msg.contains("iban") && attempt + 1 < MAX_RETRIES {
                            continue;
                        } else {
                            return Err(ServiceError::conflict(
                                "account",
                                "duplicate detected",
                            ));
                        }
                    } else {
                        return Err(ServiceError::from(RepoError::Db(sqlx::Error::Database(db_err))));
                    }
                }
                Err(e) => return Err(ServiceError::from(e)),
            }
        }

        // If we exhausted retries, surface a conflict
        Err(ServiceError::conflict("account", "unable to allocate a unique IBAN after retries"))
    }

    #[tracing::instrument(skip(self), fields(account_id = %account_id.0))]
    pub async fn get_account(&self, account_id: AccountId) -> ServiceResult<Account> {
        match self.repo.get_by_id(account_id).await {
            Ok(account) => Ok(account),
            Err(RepoError::NotFound(_)) => Err(ServiceError::not_found("account")),
            Err(e) => Err(ServiceError::from(e))
         }
    }

    pub async fn get_account_availability(
        &self,
        user_id: UserId,
    ) -> ServiceResult<AccountAvailability> {
        // 1. Load owned (type, currency) pairs
        let all_types = AccountType::all();
        let all_currencies = Currency::all();

        // One account per (type, currency). Mark each owned pair unavailable.
        let owned_pairs: HashSet<(AccountType, Currency)> = self
            .repo
            .list_type_currency_pairs(user_id)
            .await?
            .into_iter()
            .collect();

        // 2. Build available currencies per account type
        let mut available = HashMap::new();
        for &account_type in all_types {
            let free_currencies: Vec<Currency> = all_currencies
                .iter()
                .copied()
                .filter(|currency| !owned_pairs.contains(&(account_type, *currency)))
                .collect();
            available.insert(account_type, free_currencies);
        }

        Ok(AccountAvailability { available })
    }

    pub async fn list_for_user(&self, user_id: UserId) -> ServiceResult<Vec<Account>> {
        self.repo
            .list_for_user(user_id)
            .await
            .map_err(ServiceError::from)
    }
}