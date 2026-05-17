use std::sync::Arc;
use anyhow::Error as AnyError;
use argon2::{Argon2, PasswordHash, PasswordHasher, PasswordVerifier};
use argon2::password_hash::rand_core::OsRng;
use argon2::password_hash::SaltString;
use async_trait::async_trait;
use jsonwebtoken::{encode, EncodingKey, Header};
use crate::db::errors::RepoError;
use crate::db::user_repo::UserRepo;
use crate::domain::account::Account;
use crate::domain::ids::{UserId};
use crate::domain::user::User;
use crate::domain::value::email::Email;

/// Repository abstraction for user persistence
///
/// Defining this as a trait lets you unit-test `UserService` with
/// in-memory fakes instead of hitting Postgres
#[async_trait]
pub trait UserRepository: Send + Sync {
    async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>;
    async fn get_by_email(&self, email: &Email) -> Result<User, RepoError>;
    async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>;
    async fn insert(&self, user: &User) -> Result<UserId, RepoError>;
    async fn update(&self, user: &User) -> Result<(), RepoError>;
    async fn delete(&self, user_id: UserId) -> Result<(), RepoError>;
}

/// Thin adapter: your concrete SQLx repo implements the trait
///
/// This mirrors the "port/adapter" pattern and keeps the service layer
/// decoupled from SQLx's exact API
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

// Repos are thread-safe and shareable via Arc, as suggested in both Async Rust
// (custom executors sharing state) and Rust Atomics & Locks (Arc and channels).

use tokio::{task, try_join};
use tracing::{debug, info};
use tracing::log::warn;
use crate::db::account_repo::AccountRepo;
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;
pub(crate) use crate::service::account::AccountRepository;
use crate::service::auth::{Claims, LoginResult, LoginUserCommand};
use crate::service::errors::{ServiceError, ServiceResult};

/// Input DTO for registering a user at the service layer
#[derive(Debug, Clone)]
pub struct RegisterUserCommand {
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<chrono::NaiveDate>,
    /// Expect a pre-hashed password here (argon2), produced in the API layer
    pub password: String,
}

/// Composite read model: a user plus all their accounts
///
/// This is an example of how the service layer can compose multiple
/// aggregates into one response without leaking SQLx details
#[derive(Debug, Clone)]
pub struct UserWithAccounts {
    pub user: User,
    pub accounts: Vec<Account>,
}

/// Main application service for user-related use cases
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

    pub async fn login_user(
        &self,
        cmd: LoginUserCommand,
        jwt_secret: &str,
    ) -> ServiceResult<LoginResult> {
        info!(email = %cmd.email, "login attempt");

        // 1. Find the user by email
        let email: Email = cmd.email.parse().map_err(ServiceError::Domain)?;
        let user = self.user_repo.get_by_email(&email).await.map_err(|e| match e {
            RepoError::NotFound(_) => ServiceError::Validation("invalid credentials".into()),
            other => ServiceError::from(other),
        })?;

        // 2. Check the password on blocking thread
        let hash = user.password_hash().to_string();
        let password = cmd.password;

        let valid = task::spawn_blocking(move || {
            let parsed = PasswordHash::new(&hash)?;
            Argon2::default().verify_password(password.as_bytes(), &parsed)
        })
            .await
            .map_err(AnyError::new)?
            .is_ok();

        if !valid {
            warn!("login failed: invalid password");
            return Err(ServiceError::Validation("invalid credentials".into()));
        }

        // 3. Generate JWT token
        let user_id = user.id().unwrap().0;
        let claims = Claims {
            sub: user_id,
            tag: user.tag().to_string(),
            exp: (chrono::Utc::now() + chrono::Duration::hours(24)).timestamp() as usize,
        };

        let token = encode(
            &Header::default(),
            &claims,
            &EncodingKey::from_secret(jwt_secret.as_bytes()),
        )
            .map_err(AnyError::new)?;

        info!(user_id, "login successful");
        Ok(LoginResult { token, user_id })
    }

    /// Register a new user
    ///
    /// - Validates email format and domain invariants
    /// - Enforces uniqueness of `email` and `tag`
    /// - Delegates hashing to the caller (API), but *enforces* that we
    ///   only ever see a hash, never a raw password
    pub async fn register_user(&self, cmd: RegisterUserCommand) -> ServiceResult<UserId> {

        info!(email = %cmd.email, tag = %cmd.tag, "registering new user");

        // 1. Validate email format using the domain type
        debug!("checking email and tag uniqueness");
        let email: Email = cmd
            .email
            .parse()
            .map_err(ServiceError::Domain)?; // DomainError -> ServiceError

        // 2. Check uniqueness for email and tag at application level
        // Pattern: treat RepoError::NotFound as "free slot", anything else
        // is either a conflict or infrastructure failure
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

        debug!("hashing password");

        // 3. Hash the password using argon2
        let plain_password = cmd.password;
        let password_hash = task::spawn_blocking(move || -> Result<String, AnyError> {
            let salt = SaltString::generate(&mut OsRng);
            Argon2::default()
                .hash_password(plain_password.as_bytes(), &salt)
                .map(|h| h.to_string())
                .map_err(AnyError::new)   // argon2::Error -> AnyError
        })
            .await
            .map_err(AnyError::new)?  // JoinError -> AnyError -> ServiceError::Unexpected (via From)
            ?;

        // 4. Construct the domain `User`
        //
        // Following the "ownership meets invariants" idea (Zero To Production,
        // section 6.5), all heavy validations should happen as we *build*
        // the type, not afterward
        let user = User::create(
            cmd.tag,
            email,
            cmd.first_name,
            cmd.last_name,
            cmd.phone,
            cmd.birth_date,
            password_hash,
        )?;

        // 5. Persist the user through the repository
        let user_id = self.user_repo.insert(&user).await?;

        info!(user_id = user_id.0, "user registered successfully");

        // 6. Create a default account with the new user account + IBAN for it
        let mut default_account_iban = IBAN::generate()?;
        while self.account_repo.exists_by_iban(default_account_iban.as_str()).await? {
            default_account_iban = IBAN::generate()?;
        }

        let default_account = Account::create(
            user_id,
            AccountType::Regular,
            Currency::Ron,
            0,
            default_account_iban,
        )?;

        // 7. Persist the default account through the repository
        self.account_repo.insert(&default_account).await?;

        info!(user_id = user_id.0, "default user account created successfully");

        Ok(user_id)
    }

    /// Fetch a user and all their accounts **in parallel**
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

    /// Delete a user and all personal data
    pub async fn delete_user(&self, user_id: UserId) -> ServiceResult<()> {
        // check there are no non-closed accounts, etc.
        self.user_repo.delete(user_id).await?;
        Ok(())
    }

    pub async fn get_user(&self, user_id: UserId) -> ServiceResult<User> {
        self.user_repo
            .get_by_id(user_id)
            .await
            .map_err(ServiceError::from)
    }

    pub async fn find_by_tag(&self, identifier: &str) -> ServiceResult<User> {
        match self.user_repo.get_by_tag(identifier).await {
            Ok(u) => Ok(u),
            Err(RepoError::NotFound(_)) => Err(ServiceError::Validation(
                "user not found".into(),
            )),
            Err(e) => Err(ServiceError::from(e)),
        }
    }
}
