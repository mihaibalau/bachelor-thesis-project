use std::sync::Arc;
use async_trait::async_trait;
use crate::db::account_repo::AccountRepo;
use crate::db::errors::RepoError;
use crate::db::user_repo::UserRepo;
use crate::domain;
use crate::domain::account::Account;
use crate::domain::ids::{AccountId, UserId};
use crate::domain::user::User;
use crate::domain::value::email::Email;

/// Repository abstraction for user persistence.
///
/// Defining this as a trait lets you unit-test `UserService` with
/// in-memory fakes instead of hitting Postgres.
#[async_trait]
pub trait UserRepository: Send + Sync {
    async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>;
    async fn get_by_email(&self, email: &Email) -> Result<User, RepoError>;
    async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>;
    async fn insert(&self, user: &User) -> Result<UserId, RepoError>;
    async fn update(&self, user: &User) -> Result<(), RepoError>;
    async fn delete(&self, user_id: UserId) -> Result<(), RepoError>;
}

/// Thin adapter: your concrete SQLx repo implements the trait.
///
/// This mirrors the "port/adapter" pattern and keeps the service layer
/// decoupled from SQLx's exact API.
#[async_trait]
impl UserRepository for UserRepo {
    async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError> {
        self.get_by_id(user_id.0).await
    }

    async fn get_by_email(&self, email: &Email) -> Result<User, RepoError> {
        self.get_by_email(email.as_str()).await
    }

    async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError> {
        self.get_by_tag(tag).await
    }

    async fn insert(&self, user: &User) -> Result<UserId, RepoError> {
        self.insert(user).await
    }

    async fn update(&self, user: &User) -> Result<(), RepoError> {
        self.update(user).await
    }

    async fn delete(&self, user_id: UserId) -> Result<(), RepoError> {
        self.delete(user_id).await
    }
}

/// Repository abstraction for accounts.
/// Users and accounts are tightly related, so we keep this next to `UserService`.
#[async_trait]
pub trait AccountRepository: Send + Sync {
    async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>;
    async fn exists_by_account_type(
        &self,
        user_id: UserId,
        account_type: domain::value::account_type::AccountType,
    ) -> Result<bool, RepoError>;
    async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>;
    async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>;
    async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>;
}

#[async_trait]
impl AccountRepository for AccountRepo {
    async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError> {
        self.list_for_user(user_id).await
    }

    async fn exists_by_account_type(
        &self,
        user_id: UserId,
        account_type: domain::value::account_type::AccountType,
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

// Repos are thread-safe and shareable via Arc, as suggested in both Async Rust
// (custom executors sharing state) and Rust Atomics & Locks (Arc and channels).

use tokio::try_join;
use crate::service::errors::{ServiceError, ServiceResult};

/// Input DTO for registering a user at the service layer.
#[derive(Debug, Clone)]
pub struct RegisterUserCommand {
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<chrono::NaiveDate>,
    /// Expect a pre-hashed password here (argon2), produced in the API layer.
    pub password_hash: String,
}

/// Composite read model: a user plus all their accounts.
///
/// This is an example of how the service layer can compose multiple
/// aggregates into one response without leaking SQLx details.
#[derive(Debug, Clone)]
pub struct UserWithAccounts {
    pub user: User,
    pub accounts: Vec<Account>,
}

/// Main application service for user-related use cases.
#[derive(Clone)]
pub struct UserService<U, A>
where
    U: UserRepository,
    A: AccountRepository,
{
    user_repo: Arc<U>,
    account_repo: Arc<A>,
}

impl<U, A> UserService<U, A>
where
    U: UserRepository,
    A: AccountRepository,
{
    pub fn new(user_repo: Arc<U>, account_repo: Arc<A>) -> Self {
        Self {
            user_repo,
            account_repo,
        }
    }

    /// Register a new user.
    ///
    /// - Validates email format and domain invariants.
    /// - Enforces uniqueness of `email` and `tag`.
    /// - Delegates hashing to the caller (API), but *enforces* that we
    ///   only ever see a hash, never a raw password.
    pub async fn register_user(&self, cmd: RegisterUserCommand) -> ServiceResult<UserId> {
        // 1. Validate email format using the domain type.
        let email: Email = cmd
            .email
            .parse()
            .map_err(ServiceError::Domain)?; // DomainError -> ServiceError

        // 2. Check uniqueness for email and tag at application level.
        // Pattern: treat RepoError::NotFound as "free slot", anything else
        // is either a conflict or infrastructure failure.
        match self.user_repo.get_by_email(&email).await {
            Ok(_) => {
                return Err(ServiceError::conflict(
                    "user",
                    format!("email '{}' is already in use", cmd.email),
                ));
            }
            Err(RepoError::NotFound(_)) => {}
            Err(e) => return Err(ServiceError::from(e)),
        }

        match self.user_repo.get_by_tag(&cmd.tag).await {
            Ok(_) => {
                return Err(ServiceError::conflict(
                    "user",
                    format!("tag '{}' is already in use", cmd.tag),
                ));
            }
            Err(RepoError::NotFound(_)) => {}
            Err(e) => return Err(ServiceError::from(e)),
        }

        // 3. Construct the domain `User`.
        //
        // Following the "ownership meets invariants" idea (Zero To Production,
        // section 6.5), all heavy validations should happen as we *build*
        // the type, not afterward.
        let user = User::create(
            cmd.tag,
            email,
            cmd.first_name,
            cmd.last_name,
            cmd.phone,
            cmd.birth_date,
            cmd.password_hash,
        )?;

        // 4. Persist the user through the repository.
        let user_id = self.user_repo.insert(&user).await?;

        Ok(user_id)
    }

    /// Fetch a user and all their accounts **in parallel**.
    ///
    /// This uses `tokio::try_join!` to issue two independent SQL queries
    /// concurrently, inspired by the structured concurrency patterns in
    /// *Async Rust* and the "instrumenting futures" patterns in
    /// Zero To Production, section 4.5.
    pub async fn get_user_with_accounts(
        &self,
        user_id: UserId,
    ) -> ServiceResult<UserWithAccounts> {
        let user_fut = self.user_repo.get_by_id(user_id);
        let accounts_fut = self.account_repo.list_for_user(user_id);

        let (user, accounts) = try_join!(user_fut, accounts_fut)
            .map_err(ServiceError::from)?;

        Ok(UserWithAccounts { user, accounts })
    }

    /// Delete a user and all personal data.
    pub async fn delete_user(&self, user_id: UserId) -> ServiceResult<()> {
        // check there are no non-closed accounts, etc.
        self.user_repo.delete(user_id).await?;
        Ok(())
    }
}
