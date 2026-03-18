# Rust Service Layer Reference

Short technical reference for the Rust **service** layer.
Each section lists the module, purpose, main types and public methods.

The service layer sits between **domain/DB** and **API/server**:
it exposes use-cases in terms of domain types and `ServiceError`,
but knows nothing about SQLx, HTTP, or axum.

---

## Module: service/errors.rs

High-level error type for the service layer, built on top of `thiserror`.

### Types

- `pub enum ServiceError`
  - `NotFound { entity: &'static str }`
  - `Conflict { entity: &'static str, message: String }`
  - `Validation(String)`
  - `Domain(DomainError)` – wrapped via `#[error(transparent)]`
  - `Repo(RepoError)` – wrapped via `#[error(transparent)]`
  - `Concurrency(String)`
  - `Unexpected(anyhow::Error)`

- `pub type ServiceResult<T> = Result<T, ServiceError>`

### Behaviour

- Used by all service methods instead of `RepoError`.
- `NotFound` / `Conflict` make it easy for the API layer to
  map errors to HTTP 404 / 409 responses.
- `Domain` and `Repo` preserve the original error chain (for logging
  and telemetry), using `#[from]` and `#[error(transparent)]`.
- `Unexpected` is a catch-all wrapper for `anyhow::Error` when a rich
  error context is needed.

---

## Module: service/user.rs

User-related use-cases and repository abstractions.

### Traits

- `pub trait UserRepository: Send + Sync`
  - `async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>`
  - `async fn get_by_email(&self, email: &Email) -> Result<User, RepoError>`
  - `async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>`
  - `async fn insert(&self, user: &User) -> Result<UserId, RepoError>`
  - `async fn update(&self, user: &User) -> Result<(), RepoError>`
  - `async fn delete(&self, user_id: UserId) -> Result<(), RepoError>`

- `pub trait AccountRepository: Send + Sync`
  - `async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>`
  - `async fn exists_by_account_type(&self, user_id: UserId, account_type: AccountType) -> Result<bool, RepoError>`
  - `async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>`
  - `async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>`
  - `async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>`

Concrete SQLx implementations (`UserRepo`, `AccountRepo`) in the `db`
crate implement these traits via `#[async_trait]`.

### DTOs / read models

- `pub struct RegisterUserCommand`
  - `tag: String`
  - `email: String`
  - `first_name: String`
  - `last_name: String`
  - `phone: Option<String>`
  - `birth_date: Optionhrono::NaiveDate>`
  - `password_hash: String` (argon2 hash, never raw password)

- `pub struct UserWithAccounts`
  - `user: User`
  - `accounts: Vec<Account>`

### Service: `UserService<U, A>`

Generic over any `U: UserRepository` and `A: AccountRepository`.
Internally holds `Arc<U>` and `Arc<A>` for cheap, thread-safe sharing.

#### Constructors

- `pub fn new(user_repo: Arc<U>, account_repo: Arc<A>) -> Self`

#### Methods

- `pub async fn register_user(&self, cmd: RegisterUserCommand) -> ServiceResult<UserId>`
  - Validates email and domain invariants via `Email` and `User` constructors.
  - Enforces uniqueness of `email` and `tag` at application level
    (maps duplicates to `ServiceError::Conflict`).
  - Inserts the new user via `UserRepository::insert`.

- `pub async fn get_user_with_accounts(&self, user_id: UserId) -> ServiceResult<UserWithAccounts>`
  - Runs `UserRepository::get_by_id` and
    `AccountRepository::list_for_user` **in parallel** using
    `tokio::try_join!`.
  - Returns a composite read model with the user and their accounts.

- `pub async fn delete_user(&self, user_id: UserId) -> ServiceResult<()>`
  - Delegates to `UserRepository::delete`.
  - Future extension point for cascaded deletes / anonymisation.

---

## Module: service/account.rs

Account lifecycle and aggregation logic.

### Types

- `pub struct UserBalanceSummary`
  - `total_by_currency: HashMap<Currency, i64>`

  Map from currency to total balance (in cents) across all the user’s
  accounts. Designed as input for future reporting / graphs.

### Service: `AccountService<R>`

Generic over any `R: AccountRepository`, held as `Arc<R>`.

#### Constructors

- `pub fn new(repo: Arc<R>) -> Self`

#### Methods

- `pub async fn open_account(
    &self,
    user_id: UserId,
    account_type: AccountType,
    currency: Currency,
    initial_balance_cents: i64,
    iban_str: String,
) -> ServiceResult<AccountId>`
  - Parses `iban_str` into `IBAN` (domain validation).
  - Enforces:
    - user has **at most one** account per `(user_id, account_type)`.
    - IBAN is globally unique via `exists_by_iban`.
  - Constructs a new `Account` via domain constructor and inserts it.
  - Returns generated `AccountId`.

- `pub async fn get_account(&self, account_id: AccountId) -> ServiceResult<Account>`
  - Wraps `AccountRepository::get_by_id`, mapping
    `RepoError::NotFound` to `ServiceError::NotFound("account")`.

- `pub async fn compute_user_balance_summary(
    &self,
    user_id: UserId,
) -> ServiceResult<UserBalanceSummary>`
  - Loads all accounts with `list_for_user` (I/O-bound).
  - Spawns a blocking task using `tokio::task::spawn_blocking` to
    aggregate balances per currency (CPU-bound work) without blocking
    the async executor.
  - Returns a `UserBalanceSummary` suitable for analytics endpoints.

---
