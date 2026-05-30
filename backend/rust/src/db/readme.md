# Rust DB Layer Reference

Short technical reference for the Rust database layer. Each section lists the file, purpose, and public functions with signatures, parameters, behaviour, and return values.

The DB layer owns all SQL and maps rows to domain types. It never contains business rules — those live in `domain/` and `service/`.

---

## Module: `db/mod.rs`

Thin wrapper around a shared `sqlx::PgPool`.

### Types

- `pub struct Db { pool: PgPool }` – cloneable handle shared by all repositories.

### Public methods

- `pub async fn new(database_url: &str) -> Result<Self, sqlx::Error>`
  - Purpose: Create a connection pool from a PostgreSQL URL.
  - Params: `database_url` – standard Postgres connection string.
  - Returns: `Db` on success.
  - Errors: Propagates SQLx pool creation failures.

- `pub fn pool(&self) -> &PgPool`
  - Purpose: Borrow the underlying pool for `sqlx::query!` calls inside repos.

- `pub async fn ping(&self) -> Result<(), sqlx::Error>`
  - Purpose: Health-check query (`SELECT 1`).

---

## Module: `db/errors.rs`

Repository-layer error type mirroring domain `DomainError` with DB, not-found, and domain variants.

### Types

- `pub enum RepoError { Db(sqlx::Error), NotFound(&'static str), Domain(DomainError) }`
- `pub type RepoResult<T> = Result<T, RepoError>`

### Functions / impls

- `impl RepoError { pub fn not_found(entity: &'static str) -> Self }`
  - Purpose: Convenience constructor for a not-found error for a given entity.
  - Params: `entity` – static entity name (e.g. `"user"`).
  - Returns: `RepoError::NotFound(entity)`.

- `impl fmt::Display for RepoError`
  - Purpose: Human-readable formatting of repository errors.
  - Behaviour: Formats DB errors as `"db error: ..."`, not-found as `"<entity> not found"`, and domain errors as `"domain error: ..."`.

- `impl std::error::Error for RepoError`
  - Purpose: Integrate `RepoError` with the standard error trait.
  - Behaviour: Exposes underlying `sqlx::Error` or `DomainError` as source; `NotFound` has no source.

- `impl From<sqlx::Error> for RepoError`
  - Purpose: Convert any `sqlx::Error` into a `RepoError::Db` variant.
  - Typical use: `?` operator on SQLx calls within repo methods.

- `impl From<DomainError> for RepoError`
  - Purpose: Convert domain-layer validation/creation errors into `RepoError::Domain`.
  - Typical use: Propagate domain errors from `try_from` or constructors.

---

## Module: `db/account_repo.rs`

Repository for the `accounts` table. Maps rows to domain `Account` and exposes CRUD plus existence checks.

### Types

- `pub struct AccountRepo { db: Db }` – repository bound to an application `Db` wrapper.

### Public methods

- `pub fn new(db: Db) -> Self`
  - Purpose: Construct a repository bound to the given database handle.
  - Params: `db` – database abstraction providing a `sqlx::PgPool` (via `pool()`).
  - Returns: `AccountRepo`.

- `pub async fn get_by_id(&self, account_id: AccountId) -> Result<Account, RepoError>`
  - Purpose: Load a single account by primary key.
  - Params: `account_id` – domain id wrapper around `i64`.
  - Behaviour: Executes `SELECT id, user_id, account_type, currency, balance_cents, iban FROM accounts WHERE id = $1` and converts the row into `Account` via `TryFrom<AccountRow>`.
  - Errors: Returns `RepoError::NotFound("account")` if no row, `RepoError::Db` on SQL failure, or `RepoError::Domain` if rehydration fails.

- `pub async fn get_by_iban(&self, iban: &str) -> Result<Account, RepoError>`
  - Purpose: Load account by unique IBAN.
  - Params: `iban` – plain IBAN string.
  - Behaviour: Executes `SELECT ... FROM accounts WHERE iban = $1` and maps to domain `Account`.
  - Errors: Same semantics as `get_by_id`.

- `pub async fn list_for_user(&self, user_id: UserId) -> Result<Vec<Account>, RepoError>`
  - Purpose: List all accounts belonging to a given user, ordered by id.
  - Params: `user_id` – owner id.
  - Behaviour: Executes `SELECT ... FROM accounts WHERE user_id = $1 ORDER BY id`, converts each row via `Account::try_from` and collects into a `Vec<Account>`.
  - Errors: Fails on DB errors or any domain conversion error for individual rows.

- `pub async fn insert(&self, account: &Account) -> Result<AccountId, RepoError>`
  - Purpose: Insert a new account without id and return generated id.
  - Params: `account` – domain object, must not have an id set.
  - Behaviour: Validates that `account.id().is_none()`, then inserts `user_id`, `account_type`, `currency`, `balance_cents`, `iban` and returns `AccountId` from `RETURNING id`.
  - Errors: Returns domain validation error if id is already set; DB errors from SQLx (including unique violations on `iban` or `(user_id, account_type)`).

- `pub async fn update(&self, account: &Account) -> Result<(), RepoError>`
  - Purpose: Update an existing account, including `balance_cents` after a transaction.
  - Params: `account` – domain object that must carry a valid id.
  - Behaviour: Validates id presence, then runs `UPDATE accounts SET user_id = $1, account_type = $2, currency = $3, balance_cents = $4, iban = $5 WHERE id = $6`. If `rows_affected() == 0` returns not-found.
  - Errors: `RepoError::Domain` if missing id, `RepoError::Db` on DB issues, `RepoError::NotFound("account")` if no matching row.

- `pub async fn delete(&self, account_id: AccountId) -> Result<(), RepoError>`
  - Purpose: Delete account by id.
  - Params: `account_id` – account id.
  - Behaviour: Executes `DELETE FROM accounts WHERE id = $1`. If no row is affected, returns not-found.
  - Errors: DB-level errors or not-found.

- `pub async fn exists_by_iban(&self, iban: &str) -> Result<bool, RepoError>`
  - Purpose: Check if an account with a given IBAN exists.
  - Params: `iban` – IBAN string.
  - Behaviour: Executes `SELECT EXISTS (SELECT 1 FROM accounts WHERE iban = $1)` and returns `true` if any row is present.
  - Errors: Only DB-level errors.

- `pub async fn exists_by_account_type(&self, user_id: UserId, account_type: AccountType) -> Result<bool, RepoError>`
  - Purpose: Check if a user already has an account of the given type (any currency).
  - Params: `user_id` – owner; `account_type` – domain enum.
  - Behaviour: Executes `SELECT EXISTS (SELECT 1 FROM accounts WHERE account_type = $1 AND user_id = $2)` and returns `true` if at least one row exists.
  - Errors: Only DB-level errors.

- `pub async fn list_type_currency_pairs(&self, user_id: UserId) -> Result<Vec<(AccountType, Currency)>, RepoError>`
  - Purpose: Return every `(account_type, currency)` pair the user currently owns.
  - Params: `user_id` – owner id.
  - Behaviour: Executes `SELECT account_type, currency FROM accounts WHERE user_id = $1`, parses enum strings from DB rows.
  - Used by: `AccountService::get_account_availability` to determine which account **types** are already taken.
  - Errors: DB errors or domain validation if stored enum strings are invalid.

### Mapping helper

- `impl TryFrom<AccountRow> for Account`
  - Purpose: Convert raw DB row into domain `Account`.
  - Behaviour: Wraps ids into `AccountId`/`UserId`, parses `account_type` and `currency` enums and `IBAN`, then calls `Account::rehydrate`.

---

## Module: `db/user_repo.rs`

Repository for the `users` table. Handles lookups by id, email, tag and full insert/update/delete for domain `User`.

### Types

- `pub struct UserRepo { db: Db }` – repository bound to `Db`.

### Public methods

- `pub fn new(db: Db) -> Self`
  - Purpose: Create a user repository sharing the given `Db`.
  - Params: `db` – database handle.
  - Returns: `UserRepo`.

- `pub async fn get_by_id(&self, user_id: i64) -> Result<User, RepoError>`
  - Purpose: Load user by primary key.
  - Params: `user_id` – raw `i64` id (wrapped into `UserId` in mapping).
  - Behaviour: Executes `SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash FROM users WHERE id = $1` and maps into `User`.
  - Errors: Not-found, DB errors, or domain conversion errors.

- `pub async fn get_by_email(&self, email: &str) -> Result<User, RepoError>`
  - Purpose: Load user by unique email.
  - Params: `email` – email string.
  - Behaviour: Executes same projection with `WHERE email = $1`.
  - Errors: Same semantics as `get_by_id`.

- `pub async fn get_by_tag(&self, tag: &str) -> Result<User, RepoError>`
  - Purpose: Load user by public `tag` (username-like identifier).
  - Params: `tag` – tag string.
  - Behaviour: Executes same projection with `WHERE tag = $1`.
  - Errors: Same semantics as `get_by_id`.

- `pub async fn insert(&self, user: &User) -> Result<UserId, RepoError>`
  - Purpose: Insert new user without id and return generated id.
  - Params: `user` – domain object with `id == None`.
  - Behaviour: Validates missing id, inserts all user fields, and wraps the returned `id` into `UserId`.
  - Errors: Domain validation if id exists, DB errors.

- `pub async fn update(&self, user: &User) -> Result<(), RepoError>`
  - Purpose: Update existing user, including nullable `phone` and `birth_date`.
  - Params: `user` – domain object (must have id).
  - Behaviour: Validates id presence, then runs `UPDATE users SET ... WHERE id = $8`. Returns not-found if `rows_affected() == 0`.
  - Errors: Domain, DB, or not-found.

- `pub async fn delete(&self, user_id: UserId) -> Result<(), RepoError>`
  - Purpose: Delete user by id.
  - Params: `user_id` – domain id.
  - Behaviour: Executes `DELETE FROM users WHERE id = $1` and checks `rows_affected`.
  - Errors: DB or not-found.

### Mapping helper

- `impl TryFrom<UserRow> for User`
  - Purpose: Convert `UserRow` into domain `User`.
  - Behaviour: Wraps `id` into `UserId`, parses `Email`, and calls `User::rehydrate` with all fields.

---

## Module: `db/transaction_repo.rs`

Repository for the `transactions` table. Uses `chrono::DateTime<Utc>` for `recorded_on`, mirroring DB `timestamp with time zone`.

### Types

- `pub struct TransactionRepo { db: Db }` – repository using a `Db`.

### Public methods

- `pub fn new(db: Db) -> Self`
  - Purpose: Create transaction repository bound to `Db`.
  - Params: `db` – database handle.
  - Returns: `TransactionRepo`.

- `pub async fn get_by_id(&self, transaction_id: TransactionId) -> Result<Transaction, RepoError>`
  - Purpose: Load a single transaction by id.
  - Params: `transaction_id` – domain id wrapper.
  - Behaviour: Executes `SELECT id, from_account_id, to_account_id, transaction_type, value_cents, recorded_on, description FROM transactions WHERE id = $1` and maps row into `Transaction`.
  - Errors: Not-found, DB, or domain mapping errors.

- `pub async fn insert(&self, tx: &Transaction) -> Result<TransactionId, RepoError>`
  - Purpose: Insert new transaction without id and return generated id.
  - Params: `tx` – domain object, must have `id == None`.
  - Behaviour: Validates missing id, then inserts `from_account_id`, `to_account_id`, `transaction_type`, `value_cents`, `recorded_on`, `description` and returns new `TransactionId`.
  - Errors: Domain validation on existing id, DB errors.
  - Note: Balance updates happen in the **service** layer via `AccountRepo::update` before this insert is called.

- `pub async fn list_for_account(&self, account_id: AccountId, limit: i64, offset: i64) -> Result<Vec<Transaction>, RepoError>`
  - Purpose: List paginated transactions where the account is either sender or receiver.
  - Params: `account_id` – account; `limit` – max rows; `offset` – starting offset.
  - Behaviour: Executes `SELECT ... FROM transactions WHERE from_account_id = $1 OR to_account_id = $1 ORDER BY recorded_on DESC, id DESC LIMIT $2 OFFSET $3` and maps each row via `Transaction::try_from`.
  - Errors: DB errors or domain mapping failures.

### Mapping helper

- `impl TryFrom<TransactionRow> for Transaction`
  - Purpose: Convert raw DB row into domain `Transaction`.
  - Behaviour: Wraps ids into `TransactionId`/`AccountId`, parses `transaction_type` enum, and calls `Transaction::rehydrate`.

---

## Module: `db/affiliate_repo.rs`

Repository for the `affiliates` table with composite key `(owner_user_id, recipient_sub_account_id)` and domain `Affiliate`.

### Types

- `pub struct AffiliateRepo { db: Db }` – repository bound to `Db`.

### Public methods

- `pub fn new(db: Db) -> Self`
  - Purpose: Create affiliate repository using an existing `Db`.
  - Params: `db` – database handle.
  - Returns: `AffiliateRepo`.

- `pub async fn get(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<Affiliate, RepoError>`
  - Purpose: Load single affiliate by composite key.
  - Params: `owner_user_id` – owner user id; `recipient_sub_account_id` – referenced sub-account id.
  - Behaviour: Executes `SELECT owner_user_id, recipient_sub_account_id, nickname FROM affiliates WHERE owner_user_id = $1 AND recipient_sub_account_id = $2` and maps into `Affiliate`.
  - Errors: Not-found, DB, or domain errors.

- `pub async fn list_for_owner(&self, owner_user_id: UserId) -> Result<Vec<Affiliate>, RepoError>`
  - Purpose: List all affiliates owned by a user.
  - Params: `owner_user_id` – owner id.
  - Behaviour: Executes `SELECT ... FROM affiliates WHERE owner_user_id = $1 ORDER BY recipient_sub_account_id` and converts rows via `Affiliate::try_from`.
  - Errors: DB or domain mapping errors.

- `pub async fn insert(&self, affiliate: &Affiliate) -> Result<(), RepoError>`
  - Purpose: Insert a new affiliate row.
  - Params: `affiliate` – domain object.
  - Behaviour: Executes `INSERT INTO affiliates (owner_user_id, recipient_sub_account_id, nickname) VALUES ($1, $2, $3)` using domain values.
  - Errors: DB or domain errors (if constructing `Affiliate` earlier failed).

- `pub async fn update_nickname(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: &str) -> Result<(), RepoError>`
  - Purpose: Update nickname for an existing affiliate after domain validation.
  - Params: `owner_user_id` – owner; `recipient_sub_account_id` – sub-account; `nickname` – new nickname.
  - Behaviour: First calls `Affiliate::new` to validate nickname and composite key, then runs `UPDATE affiliates SET nickname = $1 WHERE owner_user_id = $2 AND recipient_sub_account_id = $3`. If no row affected, returns not-found.
  - Errors: Domain validation errors for nickname or ids, DB errors, not-found.

- `pub async fn delete(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<(), RepoError>`
  - Purpose: Delete affiliate by composite key.
  - Params: `owner_user_id` – owner; `recipient_sub_account_id` – sub-account.
  - Behaviour: Executes `DELETE FROM affiliates WHERE owner_user_id = $1 AND recipient_sub_account_id = $2` and checks `rows_affected`.
  - Errors: DB errors or not-found.

- `pub async fn exists(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<bool, RepoError>`
  - Purpose: Check if a link already exists between owner and sub-account.
  - Params: `owner_user_id` – owner; `recipient_sub_account_id` – sub-account.
  - Behaviour: Executes `SELECT 1 as "exists!" FROM affiliates WHERE owner_user_id = $1 AND recipient_sub_account_id = $2 LIMIT 1` and returns `true` if any row exists.
  - Errors: Only DB-level errors.

### Mapping helper

- `impl TryFrom<AffiliateRow> for Affiliate`
  - Purpose: Convert raw `AffiliateRow` into domain `Affiliate` via its constructor.

---

## Usage and extension notes

- All repositories follow the same pattern: keep SQL in the repo, keep invariants in the domain. New methods should:
  - Accept and return domain ids/types (`UserId`, `AccountId`, enums) instead of raw primitives when possible.
  - Use `sqlx::query!` / `query_as!` with explicit column lists and `WHERE` clauses.
  - Map result structs (`*_Row`) to domain types via `TryFrom` and domain `rehydrate` or constructors.
  - Return `RepoError::NotFound` when `fetch_optional` yields `None`, and propagate DB and domain errors via `?`.
- Service layer should never see `sqlx::Error` directly; it only deals with `RepoError` and domain types.
- To add a new function, mirror an existing method with similar behaviour (e.g. lookups by a new unique key) and keep the same error semantics (validation before SQL, translate not-found, propagate everything else).
