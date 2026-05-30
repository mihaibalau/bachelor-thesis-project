# C DB Layer Reference

Short technical reference for the C database layer. Each section lists the file, purpose, and public functions with signatures, parameters, behaviour, and return values.

The DB layer owns all SQL (via libpq) and maps rows to domain types. It never contains business rules — those live in `domain/` and `service/`.

This layer mirrors the Rust DB layer (`backend/rust/src/db/`) function-for-function.

---

## Module: `db.h` / `db.c`

Low-level wrapper around libpq `PGconn` and `PGresult` for connecting to PostgreSQL and executing parametrized text queries.

### Types

- `typedef struct Db Db;` – opaque handle for a PostgreSQL connection.

### Functions

- `bool db_connect(const char *conninfo, Db **out, RepoError *err);`
  - Purpose: Open a new PostgreSQL connection using a libpq connection string.
  - Params: `conninfo` – libpq connection string (e.g. `host=… dbname=… user=…`); `out` – receives allocated `Db*`; `err` – optional `RepoError` output.
  - Behaviour: Calls `PQconnectdb`; on failure writes connection error message into `err`.
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
  - Purpose: Execute a parametrized SQL statement with text parameters (`$1..$n`).
  - Params: `db` – connection; `sql` – SQL text; `n_params` – parameter count; `param_values` – array of C strings; `err` – optional error output.
  - Behaviour: Uses `PQexecParams`; on non-`PGRES_TUPLES_OK` / non-`PGRES_COMMAND_OK` status writes error into `err`.
  - Returns: non-`NULL` `PGresult*` on success (caller must `PQclear`), `NULL` on error.

---

## Module: `repo_error.h` / `repo_error.c`

Repository-layer error type mirroring Rust `RepoError` with DB, not-found, and domain variants.

### Types

- `typedef enum { REPO_ERROR_NONE, REPO_ERROR_DB, REPO_ERROR_NOT_FOUND, REPO_ERROR_DOMAIN } RepoErrorCode;`
- `typedef struct { RepoErrorCode code; char message[REPO_ERROR_MESSAGE_MAX]; } RepoError;`

### Functions

- `RepoError repo_error_ok(void);`
  - Purpose: Construct a `RepoError` representing success.
  - Returns: `RepoError` with `code = REPO_ERROR_NONE`.

- `RepoError repo_error_db(const char *msg);`
  - Purpose: Wrap a low-level database error string.
  - Params: `msg` – error description (may be `NULL`).
  - Returns: `RepoError` with `code = REPO_ERROR_DB`.

- `RepoError repo_error_not_found(const char *entity);`
  - Purpose: Represent a missing row for a given entity type.
  - Params: `entity` – static entity name (e.g. `"user"`).
  - Returns: `RepoError` with `code = REPO_ERROR_NOT_FOUND` and formatted message.

- `RepoError repo_error_from_domain(const DomainError *domain);`
  - Purpose: Convert a `DomainError` into a `RepoError`.
  - Params: `domain` – domain-layer error (may be `NULL`).
  - Returns: `RepoError` with `code = REPO_ERROR_DOMAIN`.

- `bool repo_error_is_ok(const RepoError *err);`
  - Purpose: Convenience check for `REPO_ERROR_NONE`.
  - Params: `err` – pointer to `RepoError` (may be `NULL`).
  - Returns: `true` if `err` is `NULL` or `code == REPO_ERROR_NONE`.

---

## Module: `util_str.h` / `util_str.c`

Small helpers for string → integer conversions used by repositories.

### Functions

- `bool util_str_to_i64(const char *s, int64_t *out);`
  - Purpose: Parse a decimal string into signed 64-bit integer.
  - Params: `s` – zero-terminated string; `out` – receives parsed value.
  - Returns: `true` if fully parsed without overflow, `false` otherwise.

---

## Module: `account_repo.h` / `account_repo.c`

Repository for the `accounts` table. Maps rows to domain `Account*` and exposes CRUD and existence checks.

### Types

- `typedef struct AccountRepo AccountRepo;` – opaque repository bound to a `Db` connection.

### Functions

- `AccountRepo *account_repo_new(Db *db);`
  - Purpose: Create a repository bound to an existing `Db` (non-owning).
  - Params: `db` – initialized database handle.
  - Returns: allocated `AccountRepo*` or `NULL` on OOM.

- `void account_repo_free(AccountRepo *repo);`
  - Purpose: Free repository instance (does not close `Db`).

- `bool account_repo_get_by_id(AccountRepo *repo, AccountId id, Account **out, RepoError *err);`
  - Purpose: Load a single account by primary key.
  - Params: `repo` – repository; `id` – account id; `out` – receives `Account*` (caller must `account_free`); `err` – error output.
  - Behaviour: `SELECT id, user_id, account_type, currency, balance_cents, iban FROM accounts WHERE id = $1`.
  - Returns: `true` on success, `false` on DB error or not-found.

- `bool account_repo_get_by_iban(AccountRepo *repo, const char *iban_str, Account **out, RepoError *err);`
  - Purpose: Load account by unique IBAN.
  - Behaviour: `SELECT … WHERE iban = $1`.

- `bool account_repo_list_for_user(AccountRepo *repo, UserId user_id, Account ***out_accounts, size_t *out_count, RepoError *err);`
  - Purpose: List all accounts belonging to a user, ordered by id.
  - Params: `out_accounts` – heap array of `Account*`; `out_count` – length.
  - Behaviour: Caller must free each account and the array pointer.

- `bool account_repo_insert(AccountRepo *repo, const Account *account, AccountId *out_id, RepoError *err);`
  - Purpose: Insert a new account without id and return generated id.
  - Params: `account` – domain object without id set.
  - Behaviour: `INSERT … RETURNING id`.
  - Returns: `false` on domain validation failure or DB error (including unique violations).

- `bool account_repo_update(AccountRepo *repo, const Account *account, RepoError *err);`
  - Purpose: Update an existing account including `balance_cents`.
  - Params: `account` – must carry valid id.
  - Behaviour: `UPDATE accounts SET … WHERE id = $n`; not-found if zero rows affected.

- `bool account_repo_delete(AccountRepo *repo, AccountId id, RepoError *err);`
  - Purpose: Delete account by id.

- `bool account_repo_exists_by_iban(AccountRepo *repo, const char *iban_str, bool *out_exists, RepoError *err);`
  - Purpose: Check if an account with a given IBAN exists.
  - Behaviour: `SELECT EXISTS (SELECT 1 FROM accounts WHERE iban = $1)`.

- `bool account_repo_exists_by_account_type(AccountRepo *repo, UserId user_id, AccountType account_type, bool *out_exists, RepoError *err);`
  - Purpose: Check if user already has **any** account of the given type (regardless of currency).
  - Behaviour: `SELECT EXISTS (SELECT 1 FROM accounts WHERE account_type = $1 AND user_id = $2)`.

---

## Module: `user_repo.h` / `user_repo.c`

Repository for the `users` table. Handles lookups by id, email, tag and full insert/update/delete for domain `User*`.

### Types

- `typedef struct UserRepo UserRepo;`

### Functions

- `UserRepo *user_repo_new(Db *db);` / `void user_repo_free(UserRepo *repo);`

- `bool user_repo_get_by_id(UserRepo *repo, UserId id, User **out, RepoError *err);`
  - Behaviour: `SELECT id, tag, email, first_name, last_name, phone, birth_date, password_hash FROM users WHERE id = $1`.

- `bool user_repo_get_by_email(UserRepo *repo, const char *email_str, User **out, RepoError *err);`
  - Behaviour: Same projection with `WHERE email = $1`.

- `bool user_repo_get_by_tag(UserRepo *repo, const char *tag, User **out, RepoError *err);`
  - Behaviour: Same projection with `WHERE tag = $1`.

- `bool user_repo_insert(UserRepo *repo, const User *user, UserId *out_id, RepoError *err);`
  - Purpose: Insert new user; rejects if id already set on domain object.

- `bool user_repo_update(UserRepo *repo, const User *user, RepoError *err);`
  - Purpose: Update user including nullable `phone` and `birth_date`.

- `bool user_repo_delete(UserRepo *repo, UserId id, RepoError *err);`
  - Purpose: Delete user by id.

---

## Module: `transaction_repo.h` / `transaction_repo.c`

Repository for the `transactions` table. Uses epoch seconds (`time_t`) for `recorded_on`, mirroring Rust `DateTime<Utc>`.

### Types

- `typedef struct TransactionRepo TransactionRepo;`

### Functions

- `TransactionRepo *transaction_repo_new(Db *db);` / `void transaction_repo_free(TransactionRepo *repo);`

- `bool transaction_repo_get_by_id(TransactionRepo *repo, TransactionId id, Transaction **out, RepoError *err);`
  - Behaviour: Full transaction projection by primary key.

- `bool transaction_repo_insert(TransactionRepo *repo, const Transaction *tx, TransactionId *out_id, RepoError *err);`
  - Purpose: Insert ledger row (balance updates happen in service layer before this call).

- `bool transaction_repo_list_for_account(TransactionRepo *repo, AccountId account_id, int64_t limit, int64_t offset, Transaction ***out_txs, size_t *out_count, RepoError *err);`
  - Purpose: Paginated list where account is sender or receiver.
  - Behaviour: `WHERE from_account_id = $1 OR to_account_id = $1 ORDER BY recorded_on DESC, id DESC LIMIT $2 OFFSET $3`.
  - Params: Caller frees each `Transaction*` and the array.

---

## Module: `affiliate_repo.h` / `affiliate_repo.c`

Repository for the `affiliates` table with composite key `(owner_user_id, recipient_sub_account_id)`.

### Types

- `typedef struct AffiliateRepo AffiliateRepo;`

### Functions

- `AffiliateRepo *affiliate_repo_new(Db *db);` / `void affiliate_repo_free(AffiliateRepo *repo);`

- `bool affiliate_repo_get(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, Affiliate **out, RepoError *err);`
  - Purpose: Load by composite key.

- `bool affiliate_repo_list_for_owner(AffiliateRepo *repo, UserId owner_user_id, Affiliate ***out_affiliates, size_t *out_count, RepoError *err);`
  - Behaviour: Ordered by `recipient_sub_account_id`.

- `bool affiliate_repo_insert(AffiliateRepo *repo, const Affiliate *affiliate, RepoError *err);`

- `bool affiliate_repo_update_nickname(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, const char *nickname, RepoError *err);`
  - Behaviour: Validates nickname via domain before `UPDATE`.

- `bool affiliate_repo_delete(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, RepoError *err);`

- `bool affiliate_repo_exists(AffiliateRepo *repo, UserId owner_user_id, AccountId recipient_sub_account_id, bool *out_exists, RepoError *err);`

---

## Usage and extension notes

- All repositories follow the same pattern as the Rust backend: SQL in the repo, invariants in the domain.
- Return `REPO_ERROR_NOT_FOUND` when a lookup finds no row; propagate DB errors as `REPO_ERROR_DB`.
- Caller owns heap-allocated domain objects returned by `get_*` and `list_*` functions — free with the matching `*_free()` helper.
- The service layer never calls libpq directly — only repository functions and `RepoError`.
- To add a new repo method, mirror the Rust equivalent: same SQL shape, same error semantics, map rows through domain constructors/`rehydrate`.
