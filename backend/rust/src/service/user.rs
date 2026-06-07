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

// Repository port for user persistence (enables in-memory fakes in tests)
#[async_trait]
pub trait UserRepository: Send + Sync {
    async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>;
    async fn get_by_email(&self, email: &Email) -> Result<User, RepoError>;
    async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>;
    async fn insert(&self, user: &User) -> Result<UserId, RepoError>;
    async fn update(&self, user: &User) -> Result<(), RepoError>;
    async fn delete(&self, user_id: UserId) -> Result<(), RepoError>;
}

// SQLx adapter implementing UserRepository
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

use tokio::{task, try_join};
use tracing::{debug, info, warn};
use crate::domain::value::account_type::AccountType;
use crate::domain::value::currency::Currency;
use crate::domain::value::iban::IBAN;
pub(crate) use crate::service::account::AccountRepository;
use crate::service::auth::{Claims, LoginResult, LoginUserCommand};
use crate::service::errors::{ServiceError, ServiceResult};

#[derive(Debug, Clone)]
pub struct RegisterUserCommand {
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<chrono::NaiveDate>,
    pub password: String,
}

#[derive(Debug, Clone)]
pub struct UserWithAccounts {
    pub user: User,
    pub accounts: Vec<Account>,
}

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

        // 1. Find the user by email (treat malformed email as invalid credentials, matching the C backend)
        let email: Email = cmd
            .email
            .parse()
            .map_err(|_| ServiceError::Validation("invalid credentials".into()))?;
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

    pub async fn register_user(&self, cmd: RegisterUserCommand) -> ServiceResult<UserId> {

        info!(email = %cmd.email, tag = %cmd.tag, "registering new user");

        // 1. Validate email format using the domain type
        debug!("checking email and tag uniqueness");
        let email: Email = cmd
            .email
            .parse()
            .map_err(ServiceError::Domain)?; // DomainError -> ServiceError

        // 2. Check uniqueness for email and tag
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

        // 4. Build domain User (validations run in constructor)
        let user = User::create(
            cmd.tag,
            email,
            cmd.first_name,
            cmd.last_name,
            cmd.phone,
            cmd.birth_date,
            password_hash,
        )?;

        // 5. Persist user
        let user_id = self.user_repo.insert(&user).await?;

        info!(user_id = user_id.0, "user registered successfully");

        // 6. Create and persist default Regular RON account
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

        self.account_repo.insert(&default_account).await?;

        info!(user_id = user_id.0, "default user account created successfully");

        Ok(user_id)
    }

    pub async fn get_user_with_accounts(
        &self,
        user_id: UserId,
    ) -> ServiceResult<UserWithAccounts> {
        // 1. Fetch user and accounts in parallel
        let user_fut = self.user_repo.get_by_id(user_id);
        let accounts_fut = self.account_repo.list_for_user(user_id);

        let (user, accounts) = try_join!(user_fut, accounts_fut)
            .map_err(ServiceError::from)?;

        Ok(UserWithAccounts { user, accounts })
    }

    pub async fn delete_user(&self, user_id: UserId) -> ServiceResult<()> {
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
