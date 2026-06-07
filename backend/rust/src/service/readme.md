# Rust Service Layer Reference

Short technical reference for the Rust **service** layer. Each section lists the module, purpose, main types, and public methods with parameters, behaviour, and return values.

The service layer sits between **domain/DB** and **server/API**: it exposes use-cases in terms of domain types and `ServiceError`, but knows nothing about SQLx, HTTP, or axum.

Both backends (Rust and C) implement the same use-cases so they can share one PostgreSQL database behind a load balancer.

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│  server/routes  (HTTP parsing, auth, JSON)       │
└─────────────────────────┬────────────────────────┘
                          │
        ┌─────────────────▼─────────────────┐
        │  service/                         │
        │  user, account, transaction,      │
        │  affiliate, auth, errors          │
        └─────────────────┬─────────────────┘
                          │
        ┌─────────────────▼─────────────────┐
        │  db/ (repos) + domain/            │
        └───────────────────────────────────┘
```

---

## Module: `service/errors.rs`

High-level error type for the service layer, built on top of `thiserror`.

### Types

- `pub enum ServiceError`
  - `NotFound { entity: &'static str }` – entity missing in storage.
  - `Conflict { entity: &'static str, message: String }` – business rule violated (duplicate email, duplicate account type, …).
  - `Validation(String)` – application-level validation (bad credentials, insufficient funds, invalid enum string from API).
  - `Domain(DomainError)` – wrapped via `#[error(transparent)]`.
  - `Repo(RepoError)` – wrapped via `#[error(transparent)]`.
  - `Concurrency(String)` – thread join / spawn failures during analytics.
  - `Forbidden` – access denied (wrong owner, transfer between different users).
  - `Unexpected(anyhow::Error)` – catch-all with rich error chain.

- `pub type ServiceResult<T> = Result<T, ServiceError>`

### Constructors / helpers

- `ServiceError::not_found(entity: &'static str) -> Self`
  - Purpose: Shorthand for `NotFound { entity }`.

- `ServiceError::conflict(entity: &'static str, msg: impl Into<String>) -> Self`
  - Purpose: Build a conflict error with human-readable message.

- `ServiceError::internal(msg: impl Into<String>) -> Self`
  - Purpose: Wrap a simple message as `Unexpected`.

- `ServiceError::with_context(self, ctx: impl Into<String>) -> ServiceError`
  - Purpose: Attach extra context while preserving the error chain.

### HTTP mapping (implemented in `server/error.rs`)

| ServiceError variant | HTTP status | JSON `code` |
|---------------------|-------------|-------------|
| NotFound | 404 | `not_found` |
| Conflict | 409 | `conflict` |
| Validation | 400 | `validation_error` |
| Domain | 400 | `domain_error` |
| Repo | 500 | `repo_error` |
| Concurrency | 409 | `concurrency_error` |
| Forbidden | 403 | `forbidden` |
| Unexpected | 500 | `unexpected_error` |

---

## Module: `service/auth.rs`

Shared auth DTOs used by `UserService` and the server layer. No standalone service struct — login logic lives in `UserService::login_user`.

### Types

- `pub struct Claims { pub sub: i64, pub tag: String, pub exp: usize }`
  - Purpose: JWT payload embedded in tokens and decoded by `server/auth.rs::require_auth`.
  - Fields: `sub` = user id; `tag` = public username; `exp` = Unix expiry timestamp.

- `pub struct LoginUserCommand { pub email: String, pub password: String }`
  - Purpose: Service-layer input for login (raw password; verified inside service).

- `pub struct LoginResult { pub token: String, pub user_id: i64 }`
  - Purpose: Output of successful login (token already signed).

---

## Module: `service/user.rs`

User registration, login, and profile read models.

### Traits

- `pub trait UserRepository: Send + Sync`
  - `async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>`
  - `async fn get_by_email(&self, email: &Email) -> Result<User, RepoError>`
  - `async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>`
  - `async fn insert(&self, user: &User) -> Result<UserId, RepoError>`
  - `async fn update(&self, user: &User) -> Result<(), RepoError>`
  - `async fn delete(&self, user_id: UserId) -> Result<(), RepoError>`

- `pub trait AccountRepository: Send + Sync` (subset used by UserService)
  - `list_for_user`, `exists_by_account_type`, `exists_by_iban`, `get_by_id`, `insert`, `update`, `list_type_currency_pairs`

Concrete SQLx implementations (`UserRepo`, `AccountRepo`) implement these traits via `#[async_trait]`.

### DTOs / read models

- `pub struct RegisterUserCommand`
  - `tag: String`
  - `email: String`
  - `first_name: String`
  - `last_name: String`
  - `phone: Option<String>`
  - `birth_date: Option<chrono::NaiveDate>`
  - `password: String` – **raw** password; hashed inside service with Argon2id.

- `pub struct UserWithAccounts`
  - `user: User`
  - `accounts: Vec<Account>`

### Service: `UserService<U, A>`

Generic over any `U: UserRepository` and `A: AccountRepository`. Internally holds `Arc<U>` and `Arc<A>`.

#### Constructors

- `pub fn new(user_repo: Arc<U>, account_repo: Arc<A>) -> Self`
  - Purpose: Wire repositories into the service.

#### Methods

- `pub async fn register_user(&self, cmd: RegisterUserCommand) -> ServiceResult<UserId>`
  - Purpose: Register a new user and provision a default bank account.
  - Behaviour:
    1. Parse and validate email via domain `Email`.
    2. Check email uniqueness (`get_by_email` → 409 if found).
    3. Check tag uniqueness (`get_by_tag` → 409 if found).
    4. Hash password with Argon2id on `spawn_blocking`.
    5. Build domain `User` via `User::create` and insert.
    6. Generate unique IBAN; create default **Regular / RON** account with balance 0; insert account.
  - Returns: New `UserId`.
  - Errors: `Conflict` on duplicate email/tag, `Domain` on invalid fields, `Repo` on DB failure.

- `pub async fn login_user(&self, cmd: LoginUserCommand, jwt_secret: &str) -> ServiceResult<LoginResult>`
  - Purpose: Authenticate user and issue JWT.
  - Behaviour:
    1. Parse email; load user by email (map not-found → `Validation("invalid credentials")`).
    2. Verify Argon2 hash on `spawn_blocking`.
    3. Build `Claims { sub, tag, exp: now + 24h }` and sign with HS256.
  - Returns: `LoginResult { token, user_id }`.
  - Errors: `Validation("invalid credentials")` on bad email/password; `Repo`/`Unexpected` on infrastructure failure.

- `pub async fn get_user_with_accounts(&self, user_id: UserId) -> ServiceResult<UserWithAccounts>`
  - Purpose: Composite read model for profile screen.
  - Behaviour: Runs `get_by_id` and `list_for_user` **in parallel** via `tokio::try_join!`.
  - Returns: `UserWithAccounts`.
  - Errors: Propagates repo/domain errors.

- `pub async fn get_user(&self, user_id: UserId) -> ServiceResult<User>`
  - Purpose: Load single user by id.
  - Returns: Domain `User`.

- `pub async fn find_by_tag(&self, identifier: &str) -> ServiceResult<User>`
  - Purpose: Resolve user by public tag.
  - Behaviour: Maps `RepoError::NotFound` → `Validation("user not found")`.

- `pub async fn delete_user(&self, user_id: UserId) -> ServiceResult<()>`
  - Purpose: Delete user row.
  - Behaviour: Delegates to `UserRepository::delete` (no HTTP route exposed yet).

---

## Module: `service/account.rs`

Account lifecycle and availability for the "open account" UI.

### Types

- `pub struct AccountAvailability { pub available: HashMap<AccountType, Vec<Currency>> }`
  - Purpose: For each account type, lists currencies the user may still open.
  - Rule: Once the user owns **any** account of a type, **all** currencies for that type are unavailable (matches `exists_by_account_type` in `open_account`).

### Trait: `AccountRepository`

- `async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>`
- `async fn exists_by_account_type(&self, user_id, account_type) -> Result<bool, RepoError>`
- `async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>`
- `async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>`
- `async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>`
- `async fn update(&self, account: &Account) -> Result<(), RepoError>`
- `async fn list_type_currency_pairs(&self, user_id) -> Result<Vec<(AccountType, Currency)>, RepoError>`

### Service: `AccountService<R>`

Generic over any `R: AccountRepository`, held as `Arc<R>`.

#### Constructors

- `pub fn new(repo: Arc<R>) -> Self`

#### Methods

- `pub async fn open_account_raw(&self, user_id, account_type_str, currency_str, initial_balance_cents) -> ServiceResult<AccountId>`
  - Purpose: Entry point from HTTP — parses raw strings from JSON body.
  - Behaviour:
    1. Parse `account_type_str` / `currency_str` via `FromStr` (validation error on fail).
    2. Reject negative `initial_balance_cents`.
    3. Delegate to `open_account`.
  - Returns: New `AccountId`.

- `pub async fn open_account(&self, user_id, account_type, currency, initial_balance_cents) -> ServiceResult<AccountId>`
  - Purpose: Open a new bank account with server-generated IBAN.
  - Behaviour:
    1. If `exists_by_account_type` → `Conflict` ("user already has an account type of …").
    2. Loop up to 5 times: generate IBAN via `IBAN::generate()`, build `Account::create`, try `insert`.
    3. On Postgres unique violation (`23505`): retry if IBAN clash; otherwise surface conflict.
  - Returns: Generated `AccountId`.
  - Errors: `Conflict`, `Domain` (invalid IBAN), `Repo`.

- `pub async fn get_account(&self, account_id: AccountId) -> ServiceResult<Account>`
  - Purpose: Load account by id for HTTP GET.
  - Behaviour: Maps `RepoError::NotFound` → `ServiceError::not_found("account")`.
  - Returns: Domain `Account`.

- `pub async fn get_account_availability(&self, user_id: UserId) -> ServiceResult<AccountAvailability>`
  - Purpose: Drive the "which account types can I still open?" UI.
  - Behaviour:
    1. Load owned `(type, currency)` pairs via `list_type_currency_pairs`.
    2. Collect distinct owned **types**.
    3. For each type in `AccountType::all()`: if owned, empty currency list; else all currencies from `Currency::all()`.
  - Returns: `AccountAvailability`.

- `pub async fn list_for_user(&self, user_id: UserId) -> ServiceResult<Vec<Account>>`
  - Purpose: Pass-through listing used internally and by affiliate resolution.

---

## Module: `service/transaction.rs`

Transaction recording, **balance updates**, statements, and analytics.

### Constants

- `BANK_ACCOUNT_ID: AccountId = AccountId(1)` – system account for ATM deposit/withdrawal and merchant payments.

### Trait: `TransactionRepository`

- `async fn get_by_id(&self, transaction_id) -> Result<Transaction, RepoError>`
- `async fn insert(&self, tx: &Transaction) -> Result<TransactionId, RepoError>`
- `async fn list_for_account(&self, account_id, limit, offset) -> Result<Vec<Transaction>, RepoError>`

### Trait: `AccountRepository` (transaction subset)

- `list_for_user`, `get_by_id`, `update`

### DTOs

- `RecordTransactionCommand` – `from_account_id`, `to_account_id`, `transaction_type`, `value_cents`, `description`
- `AccountStatementQuery` – `account_id`, optional `from`/`to` (`DateTime<Utc>`), `limit`, `offset`
- `AccountStatementEntry` – row + `balance_after_cents` (running balance)
- `UserTransactionStatistics` – totals, `per_type_totals` (gross), `per_day_totals` (signed net per day)

### Service: `TransactionService<T, A>`

#### Constructor

- `pub fn new(tx_repo: Arc<T>, account_repo: Arc<A>) -> Self`

#### Internal helper

- `async fn ensure_account_owned_by(&self, user_id, account_id) -> ServiceResult<()>`
  - Purpose: Verify account exists and belongs to user before user-facing operations.
  - Errors: `Forbidden` if wrong owner; propagates repo errors.

#### Core recording

- `pub async fn record_transaction(&self, cmd: RecordTransactionCommand) -> ServiceResult<TransactionId>`
  - Purpose: Validate, apply balance changes, persist accounts, insert ledger row.
  - Behaviour:
    1. Reject `value_cents <= 0`.
    2. Build domain `Transaction` via `Transaction::create`.
    3. Match on `transaction_type`:
       - **Deposit**: credit `to_account_id`.
       - **Withdrawal / Payment**: debit `from_account_id` if sufficient funds.
       - **Send / Transfer**: debit source + credit destination if sufficient funds.
    4. Call `account_repo.update` for each modified account.
    5. Insert transaction via `tx_repo.insert`.
  - Returns: New `TransactionId`.
  - Errors: `Validation("insufficient funds")`, domain/repo errors.

- `pub async fn record_deposit(&self, user_account_id, amount_units) -> ServiceResult<TransactionId>`
  - Purpose: ATM deposit — bank account 1 → user account.
  - Behaviour: `amount_units * 100` with `checked_mul`; description `"ATM Deposit"`.

- `pub async fn record_withdrawal(&self, user_account_id, amount_units) -> ServiceResult<TransactionId>`
  - Purpose: ATM withdrawal — user account → bank account 1.

- `pub async fn record_send(&self, from, to, value_cents, message) -> ServiceResult<TransactionId>`
  - Purpose: P2P send between two accounts.
  - Behaviour: Loads both accounts; rejects different currencies with validation error.

- `pub async fn record_transfer(&self, from, to, value_cents) -> ServiceResult<TransactionId>`
  - Purpose: Move money between user's own accounts.
  - Behaviour: Requires same `user_id` on both accounts (`Forbidden` otherwise) and same currency.

- `pub async fn record_payment(&self, from, amount_units, category, merchant, note) -> ServiceResult<TransactionId>`
  - Purpose: Card-like merchant payment to bank account 1.
  - Behaviour: Builds description `Payment | category: … | merchant: … | note: …`; uses `checked_mul(100)`.

#### User-facing wrappers (ownership-checked; called from routes)

- `pub async fn record_deposit_for_user(&self, user_id, account_id, amount_units) -> ServiceResult<TransactionId>`
- `pub async fn record_withdrawal_for_user(&self, user_id, account_id, amount_units) -> ServiceResult<TransactionId>`
- `pub async fn record_send_for_user(&self, user_id, from, recipient, value_cents, message) -> ServiceResult<TransactionId>`
- `pub async fn record_transfer_for_user(&self, user_id, from, to, value_cents) -> ServiceResult<TransactionId>`
- `pub async fn record_payment_for_user(&self, user_id, from, amount_units, category, merchant, note) -> ServiceResult<TransactionId>`
  - Each calls `ensure_account_owned_by` first, then the matching `record_*` method.

- `pub async fn list_recent_for_user(&self, user_id, account_id, limit: Option<i64>, offset: Option<i64>) -> ServiceResult<Vec<Transaction>>`
  - Purpose: Dashboard widget — latest transactions for one owned account.
  - Behaviour: Default limit 10, minimum 1; default offset 0; delegates to `list_for_account`.

- `pub async fn compute_account_statement_for_user_from_strings(&self, user_id, account_id, from_str, to_str, limit, offset) -> ServiceResult<Vec<AccountStatementEntry>>`
  - Purpose: HTTP-friendly statement entry point.
  - Behaviour: Ownership check; parse optional `YYYY-MM-DD` strings; default limit 100, offset 0; build `AccountStatementQuery`.

- `pub async fn compute_user_monthly_summary_for_user(&self, user_id, per_account_limit: Option<i64>) -> ServiceResult<UserTransactionStatistics>`
  - Purpose: Convenience wrapper without date window (route applies month bounds separately).

#### Listing and analytics

- `pub async fn list_for_account(&self, account_id, limit, offset) -> ServiceResult<Vec<Transaction>>`
  - Behaviour: Clamps `limit` to `[1, 100]`; `offset` ≥ 0.

- `pub async fn compute_account_statement(&self, query: AccountStatementQuery) -> ServiceResult<Vec<AccountStatementEntry>>`
  - Purpose: Chronological statement with running balance for graphs.
  - Behaviour (on `spawn_blocking`):
    1. Load account; read **current** `balance_cents`.
    2. Load **full** history (`limit = i64::MAX`).
    3. Compute signed delta per tx relative to statement account (`+` if `to`, `-` if `from`).
    4. Reconstruct opening balance = `current − Σ(all deltas)`; replay forward for `balance_after_cents`.
    5. Apply optional `[from, to]` date filter.
    6. Paginate (sort desc → slice → present asc).

- `pub async fn compute_user_statistics(&self, user_id, per_account_limit, from, to) -> ServiceResult<UserTransactionStatistics>`
  - Purpose: Aggregate stats across all user accounts (used by monthly summary route).
  - Behaviour (on `spawn_blocking` + `thread::scope`):
    1. Load all accounts; build set of owned account ids.
    2. Load transactions per account; **deduplicate by transaction id**.
    3. For each tx in optional `[from, to]` window: compute signed `net` relative to user's accounts.
    4. Accumulate incoming/outgoing/volume atomically; per-type (gross) and per-day (signed net) under mutex.
    5. Internal own→own transfers net to zero in totals but still appear in per-type.

---

## Module: `service/affiliate.rs`

Affiliate (saved payee) management and enriched views for the UI.

### Trait: `AffiliateRepository`

- `get`, `list_for_owner`, `insert`, `update_nickname`, `delete`, `exists`

### Service: `AffiliateService<R, A, U>`

Composes affiliate, account, and user repositories.

### DTOs

- `AffiliateView` – `recipient_sub_account_id`, `nickname`, `recipient_full_name`, `currency`
- `PaginatedAffiliatesView` – `items`, `page`, `page_size`, `total`
- `ResolveAffiliateCurrencyOptionView` – `currency`, `recipient_sub_account_id`
- `ResolvedAffiliateTargetView` – `recipient_user_id`, `recipient_full_name`, `currencies`
- `ListAffiliatesParams` – optional `page`, `page_size`, `search`, `currency`, `for_send_currency`, `sort`

### Methods

- `pub fn new(affiliate_repo, account_repo, user_repo) -> Self`

- `pub async fn create_affiliate(&self, owner, recipient_sub_account_id, nickname) -> ServiceResult<()>`
  - Behaviour: Verify recipient account exists (validation error if not); reject duplicate via `exists`; build `Affiliate::new`; insert.

- `pub async fn list_for_owner(&self, owner) -> ServiceResult<Vec<Affiliate>>`
  - Purpose: Raw affiliate entities (no enrichment).

- `pub async fn rename_affiliate(&self, owner, recipient_sub_account_id, nickname) -> ServiceResult<()>`
  - Behaviour: Delegates to `update_nickname` (domain validates nickname).

- `pub async fn delete_affiliate(&self, owner, recipient_sub_account_id) -> ServiceResult<()>`

- `pub async fn get(&self, owner, recipient_sub_account_id) -> ServiceResult<Affiliate>`
  - Behaviour: Maps not-found → `ServiceError::not_found("affiliate")`.

- `pub async fn get_affiliate_view(&self, owner, recipient_sub_account_id) -> ServiceResult<AffiliateView>`
  - Behaviour: Load affiliate; load recipient account + user for full name and currency.

- `pub async fn list_affiliates_view(&self, owner, params) -> ServiceResult<PaginatedAffiliatesView>`
  - Behaviour:
    1. Load all affiliates; enrich each with account currency and recipient full name.
    2. **Search**: if `search` trimmed length ≥ 2, filter nickname or full name (case-insensitive substring); otherwise search ignored.
    3. **Currency filter**: validate symbol; keep matching items.
    4. **Sort**: by nickname asc/desc (default asc).
    5. **Pagination**: page default 1, page_size default 20 clamped [1, 100].

- `pub async fn resolve_target_by_tag(&self, owner, tag) -> ServiceResult<ResolvedAffiliateTargetView>`
  - Behaviour:
    1. Trim tag; reject empty.
    2. Load target user by tag (not-found → validation error).
    3. Intersect owner and target account currencies.
    4. Return options with `recipient_sub_account_id` per shared currency.
    5. Error if no shared currencies.

---

## Usage and extension notes

- All services depend on repository **traits**, not concrete SQLx types (ports-and-adapters / hexagonal pattern).
- Password hashing (Argon2id) and heavy CPU work (statements, statistics) run on **blocking threads** so the async runtime stays responsive.
- Transaction analytics uses `Arc`, `Mutex`, `AtomicI64`, and `thread::scope` for parallel aggregation.
- To add a new use-case:
  - Add a method on the relevant service returning `ServiceResult<T>`.
  - Keep SQL in repository implementations.
  - Prefer domain constructors / `rehydrate` to enforce invariants at the type level.
- Business logic never belongs in `server/routes` — except the agreed **monthly-summary current-month shaping** in the route handler (mirrored in C `http_transactions.c`).
