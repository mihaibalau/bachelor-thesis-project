# C Service Layer Reference

Short technical reference for the C **service** layer. Each section lists the module, purpose, main types, and public functions with parameters, behaviour, and return values.

The service layer sits between **domain/DB** and **server/HTTP**: it exposes use-cases in terms of domain types and `ServiceError`, but knows nothing about libmicrohttpd or JSON parsing.

This layer mirrors the Rust service layer (`backend/rust/src/service/`) use-case-for-use-case so both backends can share one PostgreSQL database behind a load balancer.

---

## Architecture

```
┌──────────────────────────────────────────────────┐
│  server/  (HTTP parsing, auth, JSON)             │
└─────────────────────────┬────────────────────────┘
                          │
        ┌─────────────────▼─────────────────┐
        │  service/                         │
        │  user_service, account_service,   │
        │  transaction_service,             │
        │  affiliate_service, service_error │
        └─────────────────┬─────────────────┘
                          │
        ┌─────────────────▼─────────────────┐
        │  db/ (repos) + domain/            │
        └───────────────────────────────────┘
```

Polymorphism is implemented with explicit **vtables** (`*RepositoryVTable`) instead of Rust traits — a deliberate thesis comparison point (hand-rolled dynamic dispatch vs `async_trait`).

---

## Module: `service_error.h` / `service_error.c`

High-level error type for the service layer, mirroring Rust `ServiceError`.

### Types

- `typedef enum { SERVICE_ERROR_NONE, SERVICE_ERROR_NOT_FOUND, SERVICE_ERROR_CONFLICT, SERVICE_ERROR_VALIDATION, SERVICE_ERROR_DOMAIN, SERVICE_ERROR_REPO, SERVICE_ERROR_CONCURRENCY, SERVICE_ERROR_FORBIDDEN, SERVICE_ERROR_UNEXPECTED } ServiceErrorCode`
- `typedef struct { ServiceErrorCode code; char message[SERVICE_ERROR_MESSAGE_MAX]; } ServiceError`

### Functions

- `ServiceError service_error_ok(void);`
  - Purpose: Success sentinel (`SERVICE_ERROR_NONE`).

- `ServiceError service_error_not_found(const char *entity);`
  - Purpose: Entity missing in storage.
  - Behaviour: Formats message as `"<entity> not found"` (matches Rust).

- `ServiceError service_error_conflict(const char *entity, const char *reason);`
  - Purpose: Business conflict (duplicate email, duplicate account type, …).
  - Behaviour: Formats `"<entity> conflict: <reason>"` for HTTP layer.

- `ServiceError service_error_validation(const char *msg);`
  - Purpose: Application validation (bad credentials, insufficient funds, invalid enum string).

- `ServiceError service_error_from_domain(const DomainError *derr);`
  - Purpose: Wrap domain-layer parse/creation failures.

- `ServiceError service_error_from_repo(const RepoError *rerr);`
  - Purpose: Translate repo errors at the service boundary.
  - Behaviour: `REPO_ERROR_NOT_FOUND` → `SERVICE_ERROR_NOT_FOUND`; `REPO_ERROR_DOMAIN` → `SERVICE_ERROR_DOMAIN`; DB → `SERVICE_ERROR_REPO`.

- `ServiceError service_error_concurrency(const char *msg);`
  - Purpose: Thread join / worker failures during analytics.

- `ServiceError service_error_forbidden(void);`
  - Purpose: Access denied (wrong account owner, cross-user transfer).

- `ServiceError service_error_internal(const char *msg);`
  - Purpose: Unexpected internal failure (`SERVICE_ERROR_UNEXPECTED`).

- `bool service_error_is_ok(const ServiceError *err);`

### HTTP mapping (see `server/http_error.c`)

Identical status/code table as Rust `server/error.rs` (404 `not_found`, 409 `conflict`, 400 `validation_error`, etc.).

---

## Module: `user_service.h` / `user_service.c`

User registration, login, and profile read models.

### Repository ports (vtables)

- `UserRepositoryVTable` / `UserRepository`
  - `get_by_id`, `get_by_email`, `get_by_tag`, `insert`, `update`, `delete_`
  - Purpose: Decouple service from concrete `UserRepo`.

- `AccountRepositoryVTable` / `AccountRepository` (subset for registration)
  - `list_for_user`, `exists_by_iban`, `insert`
  - Purpose: Create default account on registration.

Adapters: `user_repository_from_user_repo`, `account_repository_from_account_repo`.

### DTOs

- `RegisterUserCommand` – `tag`, `email`, names, optional `phone`/`birth_date`, raw `password`.
- `LoginUserCommand` – `email`, `password`.
- `LoginUserResult` – `user_id`, `tag` (JWT built in server via `jwt_encode_user_id`).
- `UserWithAccounts` – owned `User*` + array of `Account*`.

### Service object: `UserService`

- `UserService *user_service_new(UserRepository user_repo, AccountRepository account_repo);`
- `void user_service_free(UserService *svc);`

- `bool user_service_register_user(UserService *svc, const RegisterUserCommand *cmd, UserId *out_user_id, ServiceError *err);`
  - Purpose: Register user + default Regular/RON account.
  - Behaviour (matches Rust `register_user`):
    1. Parse/validate email via domain `Email`.
    2. Check email and tag uniqueness → `SERVICE_ERROR_CONFLICT`.
    3. Hash password with **Argon2id** (inside service, not server).
    4. `User` domain create + insert.
    5. Generate unique IBAN; create default account; insert.
  - Returns: `true` + `*out_user_id` on success.

- `bool user_service_login_user(UserService *svc, const LoginUserCommand *cmd, LoginUserResult *out, ServiceError *err);`
  - Purpose: Verify credentials.
  - Behaviour: Load by email (not-found → validation `"invalid credentials"`); verify Argon2 hash; fill `out->user_id` and `out->tag`.
  - Note: JWT signing is intentionally in `jwt_utils.c` (server layer), unlike Rust where `login_user` returns the token.

- `bool user_service_get_user_with_accounts(UserService *svc, UserId user_id, UserWithAccounts *out, ServiceError *err);`
  - Purpose: Profile read model.
  - Behaviour: Sequential load user + account list (C has no async `try_join!`).

- `bool user_service_delete_user(UserService *svc, UserId user_id, ServiceError *err);`
  - Purpose: Delete user (no HTTP route exposed yet).

---

## Module: `account_service.h` / `account_service.c`

Account lifecycle and availability.

### Repository port

- `AccountServiceRepositoryVTable` – `list_for_user`, `exists_by_account_type`, `exists_by_iban`, `get_by_id`, `insert`

Adapter: `account_service_repository_from_repo`.

### DTOs

- `OpenAccountCommand` – `user_id`, parsed enums, `initial_balance_cents`, optional IBAN string (low-level path).
- `AccountAvailability` – `bool available[ACCOUNT_TYPE_COUNT][CURRENCY_COUNT]`.
  - Rule: once user owns **any** account of a type, **all** currencies for that type become `false`.

### Service object: `AccountService`

- `AccountService *account_service_new(AccountServiceRepository repo);`
- `void account_service_free(AccountService *svc);`

- `bool account_service_open_account(AccountService *svc, const OpenAccountCommand *cmd, AccountId *out_id, ServiceError *err);`
  - Purpose: Open account with caller-supplied IBAN (tests / low-level).
  - Behaviour: Parse IBAN; check type uniqueness; check IBAN uniqueness; domain create + insert.

- `bool account_service_open_account_raw(AccountService *svc, UserId user_id, const char *account_type_str, const char *currency_str, int64_t initial_balance_cents, AccountId *out_id, ServiceError *err);`
  - Purpose: HTTP entry point — parse strings, generate IBAN.
  - Behaviour:
    1. `account_type_from_str` / `currency_from_str`.
    2. Reject negative opening balance.
    3. `exists_by_account_type` → conflict if true.
    4. Loop up to 5 times: `iban_generate`, check `exists_by_iban`, insert; retry on collision.

- `bool account_service_get_account_availability(AccountService *svc, UserId user_id, AccountAvailability *out, ServiceError *err);`
  - Purpose: Drive open-account UI.
  - Behaviour: Start all `true`; `list_for_user`; for each owned account mark entire type row unavailable.

- `bool account_service_get_account(AccountService *svc, AccountId account_id, Account **out, ServiceError *err);`
  - Purpose: Load single account.
  - Behaviour: Maps `REPO_ERROR_NOT_FOUND` → `SERVICE_ERROR_NOT_FOUND`.

---

## Module: `transaction_service.h` / `transaction_service.c`

Transaction recording, balance updates, statements, and analytics.

### Repository ports

- `TransactionRepositoryVTable` – `get_by_id`, `insert`, `list_for_account`
- `TxAccountRepositoryVTable` – `list_for_user`, `get_by_id`, `update`

Adapters: `tx_repository_from_repo`, `tx_account_repository_from_repo`.

Bank system account id = **1** (same as Rust `BANK_ACCOUNT_ID`).

### DTOs

- `RecordTransactionCommand`, `AccountStatementQuery`, `AccountStatementEntry`
- `UserTransactionStatistics`, `PerTypeTotals`, `DayTotal` (sorted dynamic array mirroring Rust `BTreeMap`)

### Service object: `TransactionService`

- `TransactionService *transaction_service_new(TransactionRepository, TxAccountRepository);`
- `void transaction_service_free(TransactionService *svc);`

#### Core recording

- `bool transaction_service_record_transaction(TransactionService *svc, const RecordTransactionCommand *cmd, TransactionId *out_id, ServiceError *err);`
  - Purpose: Validate, apply balances, persist accounts, insert row.
  - Behaviour (identical to Rust `record_transaction`):
    - Reject non-positive `value_cents`.
    - Switch on type: credit/debit with insufficient-funds check.
    - `tx_acct_repo_update` then `tx_repo insert`.

#### Analytics (CPU-heavy, inline — no async executor)

- `bool transaction_service_list_for_account(...)` – clamp limit `[1, 100]`.
- `bool transaction_service_compute_account_statement(...)` – full history, signed deltas, opening balance reconstruction, date filter + pagination after replay.
- `bool transaction_service_compute_user_statistics(..., bool has_from, time_t from, bool has_to, time_t to, ...)` – dedupe by id, signed net, pthread workers + atomics + mutex.
- `void user_transaction_statistics_free(UserTransactionStatistics *stats);`

#### User-facing wrappers (ownership-checked)

- `transaction_service_record_deposit_for_user`
- `transaction_service_record_withdrawal_for_user`
- `transaction_service_record_send_for_user` – same-currency check
- `transaction_service_record_transfer_for_user` – same owner + currency
- `transaction_service_record_payment_for_user` – `checked_mul(100)` for cents
- `transaction_service_list_recent_for_user` – default limit 10
- `transaction_service_compute_account_statement_for_user_from_strings` – parse `YYYY-MM-DD`, ownership check

Each `*_for_user` calls internal ownership verification before delegating to the low-level `record_*` logic.

---

## Module: `affiliate_service.h` / `affiliate_service.c`

Saved payee management and enriched views.

### Repository ports

- `AffiliateRepositoryVTable` – full CRUD + `exists`
- `AffSvcAccountRepositoryVTable` – `get_by_id`, `list_for_user`
- `AffSvcUserRepositoryVTable` – `get_by_id`, `get_by_tag`

Adapters: `aff_repository_from_repo`, `aff_account_repository_from_repo`, `aff_user_repository_from_repo`.

### DTOs

- `AffiliateView`, `PaginatedAffiliatesView`, `ResolvedAffiliateTargetView`, `ListAffiliatesParams`

### Service object: `AffiliateService`

- `AffiliateService *affiliate_service_new(AffiliateRepository, AffSvcAccountRepository, AffSvcUserRepository);`
- `void affiliate_service_free(AffiliateService *svc);`

- `bool affiliate_service_create_affiliate(...)` – verify account exists; reject duplicate; domain create + insert.
- `bool affiliate_service_list_for_owner(...)` – raw affiliate entities.
- `bool affiliate_service_rename_affiliate(...)` – nickname validation via repo/domain.
- `bool affiliate_service_delete_affiliate(...)`
- `bool affiliate_service_get_affiliate_view(...)` – enrich with recipient name + currency.
- `bool affiliate_service_list_affiliates_view(...)` – search (min 2 chars), currency filter, sort, pagination (same algorithm as Rust).
- `bool affiliate_service_resolve_target_by_tag(...)` – intersect shared currencies between owner and target user.

### View cleanup

- `void paginated_affiliates_view_free(PaginatedAffiliatesView *view);`
- `void resolved_affiliate_target_view_free(ResolvedAffiliateTargetView *view);`

---

## Usage and extension notes

- Services call repository **vtables**, never libpq or SQL directly.
- Password hashing (Argon2id) lives in `user_service.c`, not the server layer.
- Heavy statistics use **pthreads** + C11 atomics instead of Rust's `spawn_blocking` / `thread::scope` — same algorithm, different concurrency model.
- Return convention: `bool` success flag + optional `ServiceError *err` out-parameter (Rust uses `Result<T, ServiceError>`).
- To add a use-case: add a `*_service_*` function, keep SQL in repos, enforce invariants via domain constructors.
- Business logic never belongs in `server/` — except the agreed **monthly-summary current-month shaping** in `http_transactions.c` (mirrored in Rust routes).
