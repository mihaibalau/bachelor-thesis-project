# C DB Layer Reference

Short technical reference for the C database layer. Each section lists the file, purpose, and public functions with signatures, parameters, and return value.

---

## Module: db.h / db.c

Low-level wrapper around libpq `PGconn` and `PGresult` for connecting to PostgreSQL and executing parametrized text queries.

### Types
- `typedef struct Db Db;` – opaque handle for a PostgreSQL connection.

### Functions
- `bool db_connect(const char *conninfo, Db **out, RepoError *err);`
  - Purpose: Open a new PostgreSQL connection using a libpq connection string.
  - Params: `conninfo` – libpq connection string; `out` – receives allocated `Db*`; `err` – optional `RepoError` output.
  - Returns: `true` on success, `false` on failure.

- `void db_close(Db *db);`
  - Purpose: Close the PostgreSQL connection and free the `Db` wrapper.
  - Params: `db` – connection handle (may be `NULL`).
  - Returns: nothing.

- `PGconn *db_raw_connection(Db *db);`
  - Purpose: Expose underlying `PGconn*` for advanced libpq usage.
  - Params: `db` – connection handle.
  - Returns: `PGconn*` or `NULL`.

- `PGresult *db_exec_params(Db *db, const char *sql, int n_params, const char *const *param_values, RepoError *err);`
  - Purpose: Execute a parametrized SQL statement with text parameters.
  - Params: `db` – connection; `sql` – SQL text with `$1..$n`; `n_params` – number of parameters; `param_values` – array of `const char*` values; `err` – optional error output.
  - Returns: non-`NULL` `PGresult*` on success (caller must `PQclear`), `NULL` on error.

---

## Module: repo_error.h / repo_error.c

Repository-layer error type mirroring Rust `RepoError` with DB, not-found, and domain variants.

### Types
- `typedef enum { REPO_ERROR_NONE, REPO_ERROR_DB, REPO_ERROR_NOT_FOUND, REPO_ERROR_DOMAIN } RepoErrorCode;`
- `typedef struct { RepoErrorCode code; char message[REPO_ERROR_MESSAGE_MAX]; } RepoError;`

### Functions
- `RepoError repo_error_ok(void);`
  - Purpose: Construct a `RepoError` representing success.
  - Params: none.
  - Returns: `RepoError` with `code = REPO_ERROR_NONE`.

- `RepoError repo_error_db(const char *msg);`
  - Purpose: Wrap a low-level database error string.
  - Params: `msg` – error description (may be `NULL`).
  - Returns: `RepoError` with `code = REPO_ERROR_DB`.

- `RepoError repo_error_not_found(const char *entity);`
  - Purpose: Represent a missing row for a given entity type.
  - Params: `entity` – static entity name (e.g. "user").
  - Returns: `RepoError` with `code = REPO_ERROR_NOT_FOUND`.

- `RepoError repo_error_from_domain(const DomainError *domain);`
  - Purpose: Convert a `DomainError` into a `RepoError`.
  - Params: `domain` – domain-layer error (may be `NULL`).
  - Returns: `RepoError` with `code = REPO_ERROR_DOMAIN`.

- `bool repo_error_is_ok(const RepoError *err);`
  - Purpose: Convenience check for `REPO_ERROR_NONE`.
  - Params: `err` – pointer to `RepoError` (may be `NULL`).
  - Returns: `true` if `err` is `NULL` or `code == REPO_ERROR_NONE`.

---

## Module: util_str.h / util_str.c

Small helpers for string → integer conversions used by repositories.

### Functions
- `bool util_str_to_i64(const char *s, int64_t *out);`
  - Purpose: Parse a decimal string into signed 64-bit integer.
  - Params: `s` – zero-terminated string; `out` – receives parsed value.
  - Returns: `true` if fully parsed without overflow, `false` otherwise.

---

## Module: account_repo.h / account_repo.c

Repository for `accounts` table. Maps rows to domain `Account*` and exposes CRUD and existence checks.

### Types
- `typedef struct AccountRepo AccountRepo;` – opaque repository bound to a `Db` connection.

### Functions
- `AccountRepo *account_repo_new(Db *db);`
  - Purpose: Create a repository bound to an existing `Db` (non-owning).
  - Params: `db` – initialized database handle.
  - Returns: allocated `AccountRepo*` or `NULL` on OOM.

- `void account_repo_free(AccountRepo *repo);`
  - Purpose: Free repository instance (does not close `Db`).
  - Params: `repo` – repository pointer.
  - Returns: nothing.

- `bool account_repo_get_by_id(AccountRepo *repo, AccountId id, Account **out, RepoError *err);`
  - Purpose: Load a single account by primary key.
  - Params: `repo` – repository; `id` – account id; `out` – receives `Account*` (must `account_free`); `err` – error output.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool account_repo_get_by_iban(AccountRepo *repo, const char *iban_str, Account **out, RepoError *err);`
  - Purpose: Load account by unique IBAN.
  - Params: `repo` – repository; `iban_str` – plain IBAN string; `out` – receives `Account*`; `err` – error.
  - Returns: `true` on success, `false` otherwise.

- `bool account_repo_list_for_user(AccountRepo *repo, UserId user_id, Account ***out_accounts, size_t *out_count, RepoError *err);`
  - Purpose: List all accounts belonging to a user.
  - Params: `repo` – repository; `user_id` – owner; `out_accounts` – receives array of `Account*`; `out_count` – array length; `err` – error.
  - Returns: `true` on success, `false` otherwise.

- `bool account_repo_insert(AccountRepo *repo, const Account *account, AccountId *out_id, RepoError *err);`
  - Purpose: Insert a new account without id and return generated id.
  - Params: `repo` – repository; `account` – domain object; `out_id` – receives `AccountId`; `err` – error.
  - Returns: `true` on success, `false` on DB or domain error.

- `bool account_repo_update(AccountRepo *repo, const Account *account, RepoError *err);`
  - Purpose: Update an existing account (must have id).
  - Params: `repo` – repository; `account` – domain object; `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool account_repo_delete(AccountRepo *repo, AccountId id, RepoError *err);`
  - Purpose: Delete account by id.
  - Params: `repo` – repository; `id` – account id; `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool account_repo_exists_by_iban(AccountRepo *repo, const char *iban_str, bool *out_exists, RepoError *err);`
  - Purpose: Check if an account with a given IBAN exists.
  - Params: `repo` – repository; `iban_str` – IBAN; `out_exists` – receives boolean; `err` – error.
  - Returns: `true` if query executed, `false` on DB error.

- `bool account_repo_exists_by_account_type(AccountRepo *repo, UserId user_id, AccountType account_type, bool *out_exists, RepoError *err);`
  - Purpose: Check if user already has an account of a given type.
  - Params: `repo` – repository; `user_id` – owner; `account_type` – enum; `out_exists` – boolean out; `err` – error.
  - Returns: `true` if query executed, `false` on DB error.

---

## Module: user_repo.h / user_repo.c

Repository for `users` table. Handles lookups by id, email, tag and full insert/update/delete for domain `User*`.

### Types
- `typedef struct UserRepo UserRepo;` – repository bound to a `Db`.

### Functions
- `UserRepo *user_repo_new(Db *db);`
  - Purpose: Create a user repository sharing the given `Db`.
  - Params: `db` – database handle.
  - Returns: `UserRepo*` or `NULL`.

- `void user_repo_free(UserRepo *repo);`
  - Purpose: Free repository structure.
  - Params: `repo` – repository pointer.
  - Returns: nothing.

- `bool user_repo_get_by_id(UserRepo *repo, UserId id, User **out, RepoError *err);`
  - Purpose: Load user by primary key.
  - Params: `repo` – repository; `id` – user id; `out` – receives `User*` (must `user_free`); `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool user_repo_get_by_email(UserRepo *repo, const char *email_str, User **out, RepoError *err);`
  - Purpose: Load user by unique email.
  - Params: `repo` – repository; `email_str` – email; `out` – receives `User*`; `err` – error.
  - Returns: `true` on success, `false` otherwise.

- `bool user_repo_get_by_tag(UserRepo *repo, const char *tag, User **out, RepoError *err);`
  - Purpose: Load user by public "tag" (username-like).
  - Params: `repo` – repository; `tag` – tag string; `out` – receives `User*`; `err` – error.
  - Returns: `true` on success, `false` otherwise.

- `bool user_repo_insert(UserRepo *repo, const User *user, UserId *out_id, RepoError *err);`
  - Purpose: Insert new user without id and return generated id.
  - Params: `repo` – repository; `user` – domain object; `out_id` – receives `UserId`; `err` – error.
  - Returns: `true` on success, `false` on DB or domain error.

- `bool user_repo_update(UserRepo *repo, const User *user, RepoError *err);`
  - Purpose: Update existing user, including nullable phone and birth_date.
  - Params: `repo` – repository; `user` – domain object (must have id); `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool user_repo_delete(UserRepo *repo, UserId id, RepoError *err);`
  - Purpose: Delete user by id.
  - Params: `repo` – repository; `id` – user id; `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

---

## Module: transaction_repo.h / transaction_repo.c

Repository for `transactions` table. Uses epoch seconds in C for `recorded_on`, mirroring Rust `DateTime<Utc>`.

### Types
- `typedef struct TransactionRepo TransactionRepo;` – repository using a `Db`.

### Functions
- `TransactionRepo *transaction_repo_new(Db *db);`
  - Purpose: Create transaction repository bound to `Db`.
  - Params: `db` – database handle.
  - Returns: `TransactionRepo*` or `NULL`.

- `void transaction_repo_free(TransactionRepo *repo);`
  - Purpose: Free repository structure.
  - Params: `repo` – repository pointer.
  - Returns: nothing.

- `bool transaction_repo_get_by_id(TransactionRepo *repo, TransactionId id, Transaction **out, RepoError *err);`
  - Purpose: Load a single transaction by id.
  - Params: `repo` – repository; `id` – transaction id; `out` – receives `Transaction*` (must `transaction_free`); `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool transaction_repo_insert(TransactionRepo *repo, const Transaction *tx, TransactionId *out_id, RepoError *err);`
  - Purpose: Insert new transaction without id and return generated id.
  - Params: `repo` – repository; `tx` – domain object; `out_id` – receives `TransactionId`; `err` – error.
  - Returns: `true` on success, `false` on DB or domain error.

- `bool transaction_repo_list_for_account(TransactionRepo *repo, AccountId account_id, int64_t limit, int64_t offset, Transaction ***out_txs, size_t *out_count, RepoError *err);`
  - Purpose: List paginated transactions where the account participates as sender or receiver.
  - Params: `repo` – repository; `account_id` – account; `limit` – max rows; `offset` – starting offset; `out_txs` – receives array of `Transaction*`; `out_count` – length; `err` – error.
  - Returns: `true` on success, `false` otherwise.

---

## Module: affiliate_repo.h / affiliate_repo.c

Repository for `affiliates` table with composite key `(owner_user_id, recipient_sub_account_id)` and domain `Affiliate*`.

### Types
- `typedef struct AffiliateRepo AffiliateRepo;` – repository bound to `Db`.

### Functions
- `AffiliateRepo *affiliate_repo_new(Db *db);`
  - Purpose: Create affiliate repository using an existing `Db`.
  - Params: `db` – database handle.
  - Returns: `AffiliateRepo*` or `NULL`.

- `void affiliate_repo_free(AffiliateRepo *repo);`
  - Purpose: Free repository structure.
  - Params: `repo` – repository pointer.
  - Returns: nothing.

- `bool affiliate_repo_get(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, Affiliate **out, RepoError *err);`
  - Purpose: Load single affiliate by composite key.
  - Params: `repo` – repository; `owner_user_id` – owner; `recipient_sub_account_id` – sub-account; `out` – receives `Affiliate*` (must `affiliate_free`); `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool affiliate_repo_list_for_owner(AffiliateRepo *repo, UserId owner_user_id, Affiliate ***out_affiliates, size_t *out_count, RepoError *err);`
  - Purpose: List all affiliates owned by a user.
  - Params: `repo` – repository; `owner_user_id` – owner; `out_affiliates` – array of `Affiliate*`; `out_count` – length; `err` – error.
  - Returns: `true` on success, `false` otherwise.

- `bool affiliate_repo_insert(AffiliateRepo *repo, const Affiliate *affiliate, RepoError *err);`
  - Purpose: Insert a new affiliate row.
  - Params: `repo` – repository; `affiliate` – domain object; `err` – error.
  - Returns: `true` on success, `false` on DB or domain error.

- `bool affiliate_repo_update_nickname(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, const char *nickname, RepoError *err);`
  - Purpose: Update nickname for an existing affiliate after domain validation.
  - Params: `repo` – repository; `owner_user_id` – owner; `recipient_sub_account_id` – sub-account; `nickname` – new nickname; `err` – error.
  - Returns: `true` on success, `false` on DB error, domain error, or not-found.

- `bool affiliate_repo_delete(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, RepoError *err);`
  - Purpose: Delete affiliate by composite key.
  - Params: `repo` – repository; `owner_user_id` – owner; `recipient_sub_account_id` – sub-account; `err` – error.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool affiliate_repo_exists(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, bool *out_exists, RepoError *err);`
  - Purpose: Check if a link already exists between owner and sub-account.
  - Params: `repo` – repository; `owner_user_id` – owner; `recipient_sub_account_id` – sub-account; `out_exists` – boolean out; `err` – error.
  - Returns: `true` if query executed, `false` on DB error.
