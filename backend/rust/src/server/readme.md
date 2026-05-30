# Rust Server Layer Reference

The **server** layer is the HTTP/API boundary. It accepts requests, enforces JWT auth on private routes, deserializes JSON, calls the **service** layer, and serializes responses. It contains **no business logic** — parsing, routing, auth extraction, and JSON mapping only.

Both backends (Rust and C) expose the same routes, request bodies, response shapes, and error codes so they can run behind one load balancer with one frontend and one database.

**Stack:** [axum](https://github.com/tokio-rs/axum) + `tower-http` (CORS, tracing).

---

## Architecture

```
                ┌──────────────────────────────┐
                │         main.rs              │
                │  (Db, repos, services,       │
                │   AppState, create_router)   │
                └──────────────┬───────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      server/router.rs        │
                │  nest /api/{users,accounts,  │
                │  affiliates,transactions}    │
                └──────┬───────────┬───────────┘
                       │           │
       ┌───────────────▼──┐   ┌────▼────────────────────────────┐
       │  server/auth.rs  │   │  server/routes/{users,accounts, │
       │  require_auth    │   │  affiliates,transactions}.rs    │
       └──────────────────┘   └──────────────┬────────────────────┘
                                             │
                              ┌──────────────▼──────────────┐
                              │  service/ (use-cases)       │
                              └─────────────────────────────┘
```

---

## Module reference

Each file below lists public items with purpose, behaviour, and what the handler delegates to the service layer. **No business rules** live here — only HTTP concerns.

---

### `server/router.rs`

- `pub fn create_router(state: Arc<AppState>) -> Router`
  - Purpose: Assemble the full application router.
  - Behaviour:
    - Nests `routes::users::router`, `routes::accounts::router`, `routes::affiliates::router`, `routes::transactions::router` under `/api/...`.
    - Applies `CorsLayer::permissive()` and `TraceLayer::new_for_http()`.
    - Shares `AppState` via `.with_state(state)`.

---

### `server/state.rs`

- `pub struct AppState`
  - Fields: `user_svc: Arc<UserSvc>`, `account_svc: Arc<AccountSvc>`, `tx_svc: Arc<TxSvc>`, `affiliate_svc: Arc<AffiliateSvc>`, `jwt_secret: String`.
  - Purpose: Dependency injection container passed to every handler and middleware.

- Type aliases:
  - `UserSvc = UserService<UserRepo, AccountRepo>`
  - `AccountSvc = AccountService<AccountRepo>`
  - `TxSvc = TransactionService<TransactionRepo, AccountRepo>`
  - `AffiliateSvc = AffiliateService<AffiliateRepo, AccountRepo, UserRepo>`

- `impl AppState { pub fn new(user_svc, account_svc, tx_svc, affiliate_svc, jwt_secret) -> Self }`
  - Purpose: Construct state in `main.rs` after wiring repos and services.

---

### `server/auth.rs`

- `pub async fn require_auth(State(state), mut req, next) -> Result<Response, StatusCode>`
  - Purpose: JWT middleware for all private route groups.
  - Behaviour:
    1. Read `Authorization` header; require `Bearer <token>` prefix.
    2. Decode JWT with `jsonwebtoken` + `DecodingKey::from_secret(jwt_secret)`.
    3. Insert decoded `Claims` into `req.extensions_mut()`.
    4. Call `next.run(req)`.
  - Errors: Returns bare `401 Unauthorized` (no JSON body) if header missing or token invalid/expired.

---

### `server/error.rs`

- `struct ErrorBody { status: u16, code: &'static str, message: String }` – JSON error envelope.

- `pub struct ApiError(pub ServiceError)` – newtype wrapper for `IntoResponse`.

- `impl From<ServiceError> for ApiError`

- `impl IntoResponse for ApiError`
  - Purpose: Map every `ServiceError` variant to HTTP status + `{ status, code, message }` JSON (see HTTP API section below).

- `pub type ApiResult<T> = Result<T, ApiError>` – standard handler return type.

---

### `server/routes/users.rs`

#### Router

- `pub fn router(state: Arc<AppState>) -> Router`
  - Public routes: `POST /`, `POST /login`.
  - Private routes: `GET /{id}` with `require_auth` middleware.

#### Request / response DTOs

- `LoginRequest`, `RegisterUserRequest` – deserialize JSON bodies.
- `LoginResponse { token, user_id }`, `RegisterUserResponse { user_id }`
- `UserResponse`, `AccountResponse`, `UserWithAccountsResponse` – serialize outbound JSON.

#### Handlers

- `async fn login_user(State, Json<LoginRequest>) -> ApiResult<Json<LoginResponse>>`
  - Delegates: `user_svc.login_user(LoginUserCommand, &jwt_secret)`.
  - Returns: JWT token + user id. No auth required.

- `async fn register_user(State, Json<RegisterUserRequest>) -> ApiResult<Json<RegisterUserResponse>>`
  - Parses optional `birth_date` (`YYYY-MM-DD`) in route layer.
  - Delegates: `user_svc.register_user(RegisterUserCommand)`.
  - Returns: `{ user_id }`. No auth required.

- `pub async fn get_user_with_accounts(State, Path(id), Extension(claims)) -> ApiResult<Json<UserWithAccountsResponse>>`
  - Auth: rejects with `403 Forbidden` if `claims.sub != id`.
  - Delegates: `user_svc.get_user_with_accounts(UserId)`.
  - Maps domain user + accounts into response DTOs (strips password hash).

---

### `server/routes/accounts.rs`

#### Router

- `pub fn router(state: Arc<AppState>) -> Router`
  - All routes private (`require_auth`).
  - **Route order matters**: `/availability` registered **before** `/{id}` so `"availability"` is not parsed as an integer id.

#### Request / response DTOs

- `OpenAccountRequest { account_type, currency, initial_balance_cents }`
- `AccountResponse { id, account_type, currency, balance_cents, iban }`
- `AccountAvailabilityResponse { types: Vec<AccountTypeAvailability> }` with nested `CurrencyAvailability`.

#### Handlers

- `async fn open_account(State, Extension(claims), Json(body)) -> ApiResult<Json<AccountResponse>>`
  - Delegates: `account_svc.open_account_raw(user_id, strings…)` then `get_account` to build response.
  - No IBAN in request — generated in service.

- `async fn get_account(State, Path(id), Extension(claims)) -> ApiResult<Json<AccountResponse>>`
  - Delegates: `account_svc.get_account`.
  - Auth: returns `404 not_found` if `account.user_id != claims.sub` (does not leak existence).

- `async fn get_availability(State, Extension(claims)) -> ApiResult<Json<AccountAvailabilityResponse>>`
  - Delegates: `account_svc.get_account_availability`.
  - Shapes service map into UI-friendly nested JSON with `has_any_available` per type.

---

### `server/routes/affiliates.rs`

#### Router

- `pub fn router(state: Arc<AppState>) -> Router` – all routes private.

#### Key DTOs

- `CreateAffiliateRequest { recipient_sub_account_id, nickname }`
- `ResolveAffiliateTargetRequest { identifier_type, identifier }` – `identifier_type` enum: `tag` | `phone`.
- `ListAffiliatesQuery` – `page`, `page_size`, `search`, `currency`, `sort`.
- Response types mirror service views (`AffiliateView`, paginated list, resolve-target currencies).

#### Handlers

- `async fn list_affiliates(...)` – builds `ListAffiliatesParams`; delegates `list_affiliates_view`.
- `async fn create_affiliate(...)` – delegates `create_affiliate`.
- `async fn get_affiliate(Path(sub_account_id), ...)` – delegates `get_affiliate_view`.
- `async fn update_affiliate_nickname(PATCH, ...)` – delegates `rename_affiliate`; empty 200 body.
- `async fn delete_affiliate(DELETE, ...)` – delegates `delete_affiliate`; empty 200 body.
- `async fn resolve_affiliate_target(...)` – `identifier_type::Tag` → `resolve_target_by_tag`; `Phone` → validation error (not implemented yet).

---

### `server/routes/transactions.rs`

#### Router

- `pub fn router(state: Arc<AppState>) -> Router` – all routes private.

#### Request DTOs (examples)

- `DepositRequest { account_id, amount }` – amount in whole units.
- `SendRequest { from_account_id, recipient_account_id, value_cents, message }`
- `PaymentRequest { from_account_id, amount, category, merchant_name, note }`
- Query types: `RecentQuery`, `StatementQuery`, `UserMonthlySummaryQuery`.

#### Handlers

- `record_deposit` / `record_withdrawal` / `record_send` / `record_transfer` / `record_payment`
  - Each parses JSON, calls matching `*_for_user` on `tx_svc`, returns `{ "id": <TransactionId> }`.

- `get_recent_transactions(Query)` – delegates `list_recent_for_user`; serializes transaction list.

- `get_account_statement(Query)` – delegates `compute_account_statement_for_user_from_strings`; maps entries to RFC3339 timestamps.

- `get_user_monthly_summary(Query)`
  - **Route-layer shaping (agreed exception):** computes current UTC calendar-month `[start, end]` bounds.
  - Delegates: `compute_user_statistics(user_id, per_account_limit, Some(start), Some(end))`.
  - Builds `daily_cumulative_spending` from signed per-day nets (`value.min(0).abs()` for outgoing).
  - All response totals reflect the current month only.

---

## Request lifecycle

```
Client → axum Router
       → [require_auth middleware on private nests]
       → handler: extract Path / Query / Extension(Claims) / Json body
       → AppState.{user,account,tx,affiliate}_svc method
       → Ok(Json(dto))  or  Err(ApiError(ServiceError))
       → ApiError::into_response → JSON error envelope
```

---

## HTTP API Reference

Base URL: `/api`

Private endpoints require `Authorization: Bearer <JWT>` (from `POST /api/users/login`).

### Error envelope (both backends)

```json
{
  "status": 400,
  "code": "validation_error",
  "message": "Human readable message"
}
```

| HTTP status | JSON `code` | When |
|-------------|-------------|------|
| 400 | `validation_error` / `domain_error` | Invalid input or domain rule |
| 403 | `forbidden` | Access denied |
| 404 | `not_found` | Missing resource (also used when resource exists but is not owned) |
| 409 | `conflict` / `concurrency_error` | Duplicate or coordination failure |
| 500 | `repo_error` / `unexpected_error` | Infrastructure failure |

---

### Users & Auth — `/api/users`

#### Register user

- **POST** `/api/users` (public)

Request:

```json
{
  "tag": "mihai123",
  "email": "mihai@example.com",
  "first_name": "Mihai",
  "last_name": "Balau",
  "phone": "+40712345678",
  "birth_date": "1995-05-10",
  "password": "plain-password"
}
```

Response `200 OK`:

```json
{ "user_id": 1 }
```

Creates user + default Regular/RON account with generated IBAN.

---

#### Login

- **POST** `/api/users/login` (public)

Request:

```json
{
  "email": "mihai@example.com",
  "password": "plain-password"
}
```

Response `200 OK`:

```json
{
  "token": "jwt-token-here",
  "user_id": 1
}
```

---

#### Get user with accounts

- **GET** `/api/users/{id}` (private; `{id}` must equal JWT `sub`)

Response `200 OK`:

```json
{
  "user": {
    "id": 1,
    "tag": "mihai123",
    "email": "mihai@example.com",
    "first_name": "Mihai",
    "last_name": "Balau",
    "phone": "+40712345678",
    "birth_date": "1995-05-10"
  },
  "accounts": [
    {
      "id": 17,
      "account_type": "Regular",
      "currency": "RON",
      "balance_cents": 500000,
      "iban": "RO49AAAA1B31007593840000"
    }
  ]
}
```

---

### Accounts — `/api/accounts`

#### Open account

- **POST** `/api/accounts` (private)

Request:

```json
{
  "account_type": "Savings",
  "currency": "RON",
  "initial_balance_cents": 0
}
```

Response `200 OK`:

```json
{
  "id": 42,
  "account_type": "Savings",
  "currency": "RON",
  "balance_cents": 0,
  "iban": "RO54AAAA1B31007593840001"
}
```

IBAN is generated server-side. Rule: **one account per account type** per user (not per currency). If the user already owns any Savings account, response is `409 conflict`:

```json
{
  "status": 409,
  "code": "conflict",
  "message": "account conflict: user 1 already has an account type of Savings"
}
```

---

#### Get account by id

- **GET** `/api/accounts/{id}` (private; must own account)

Response `200 OK`:

```json
{
  "id": 42,
  "account_type": "Savings",
  "currency": "RON",
  "balance_cents": 100000,
  "iban": "RO54AAAA1B31007593840001"
}
```

---

#### Get account availability

- **GET** `/api/accounts/availability` (private)

Once a user owns **any** account of a type, **all** currencies for that type show `available: false`.

Response `200 OK`:

```json
{
  "types": [
    {
      "account_type": "Regular",
      "has_any_available": false,
      "currencies": [
        { "currency": "RON", "available": false },
        { "currency": "EUR", "available": false },
        { "currency": "USD", "available": false }
      ]
    },
    {
      "account_type": "Savings",
      "has_any_available": true,
      "currencies": [
        { "currency": "RON", "available": true },
        { "currency": "EUR", "available": true },
        { "currency": "USD", "available": true }
      ]
    }
  ]
}
```

---

### Affiliates — `/api/affiliates`

#### Resolve affiliate target

- **POST** `/api/affiliates/resolve-target` (private)

Request:

```json
{
  "identifier_type": "tag",
  "identifier": "mihai123"
}
```

Response `200 OK`:

```json
{
  "recipient_user_id": 2,
  "recipient_full_name": "John Doe",
  "currencies": [
    { "currency": "RON", "recipient_sub_account_id": 77 },
    { "currency": "EUR", "recipient_sub_account_id": 78 }
  ]
}
```

`identifier_type: "phone"` currently returns `400 validation_error` ("phone identifier not supported yet").

---

#### Create affiliate

- **POST** `/api/affiliates` (private)

Request:

```json
{
  "recipient_sub_account_id": 77,
  "nickname": "John salary"
}
```

Response `200 OK` — empty body.

---

#### Get single affiliate

- **GET** `/api/affiliates/{sub_account_id}` (private)

Response `200 OK`:

```json
{
  "recipient_sub_account_id": 77,
  "nickname": "John salary",
  "recipient_full_name": "John Doe",
  "currency": "RON"
}
```

---

#### Update nickname

- **PATCH** `/api/affiliates/{sub_account_id}` (private)

Request:

```json
{ "nickname": "John – rent" }
```

Response `200 OK` — empty body.

---

#### Delete affiliate

- **DELETE** `/api/affiliates/{sub_account_id}` (private)

Response `200 OK` — empty body.

---

#### List affiliates

- **GET** `/api/affiliates` (private)

Query params (all optional):

| Param | Type | Description |
|-------|------|-------------|
| `page` | u32 | 1-based, default 1 |
| `page_size` | u32 | default 20, clamped [1, 100] |
| `search` | str | substring on nickname or recipient name; **ignored if length < 2** |
| `currency` | str | e.g. `RON`, `EUR` |
| `sort` | str | `"asc"` (default) or `"desc"` by nickname |

Example: `GET /api/affiliates?page=1&page_size=10&search=jo&currency=RON&sort=asc`

Response `200 OK`:

```json
{
  "items": [
    {
      "recipient_sub_account_id": 77,
      "nickname": "John salary",
      "recipient_full_name": "John Doe",
      "currency": "RON"
    }
  ],
  "page": 1,
  "page_size": 10,
  "total": 1
}
```

---

### Transactions — `/api/transactions`

All endpoints private. Supported `transaction_type` values: `"Deposit"`, `"Withdrawal"`, `"Send"`, `"Transfer"`, `"Payment"`.

Recording a transaction **updates account balances** and rejects insufficient funds with `400 validation_error`.

Bank system account id = **1** (deposits, withdrawals, payments).

---

#### Record deposit

- **POST** `/api/transactions/deposit`

Request (`amount` in whole units, e.g. `100` → `10000` cents):

```json
{ "account_id": 17, "amount": 100 }
```

Response `200 OK`:

```json
{ "id": 123 }
```

---

#### Record withdrawal

- **POST** `/api/transactions/withdrawal`

Request:

```json
{ "account_id": 17, "amount": 50 }
```

Response `200 OK`:

```json
{ "id": 124 }
```

---

#### Record send

- **POST** `/api/transactions/send`

Request (requires same currency on both accounts):

```json
{
  "from_account_id": 17,
  "recipient_account_id": 77,
  "value_cents": 250000,
  "message": "Rent for May"
}
```

Response `200 OK`:

```json
{ "id": 125 }
```

---

#### Record transfer

- **POST** `/api/transactions/transfer`

Request (both accounts must belong to the same user, same currency):

```json
{
  "from_account_id": 17,
  "to_account_id": 18,
  "value_cents": 100000
}
```

Response `200 OK`:

```json
{ "id": 126 }
```

---

#### Record payment

- **POST** `/api/transactions/payment`

Request:

```json
{
  "from_account_id": 17,
  "amount": 42,
  "category": "Food",
  "merchant_name": "McBurger",
  "note": "Lunch menu"
}
```

Response `200 OK`:

```json
{ "id": 127 }
```

---

#### Recent transactions

- **GET** `/api/transactions/recent?account_id=17&limit=5`

Response `200 OK`:

```json
{
  "items": [
    {
      "id": 127,
      "from_account_id": 17,
      "to_account_id": 1,
      "transaction_type": "Payment",
      "value_cents": 4200,
      "recorded_on": "2026-05-18T16:30:00Z",
      "description": "Payment | category: Food | merchant: McBurger | note: Lunch menu"
    }
  ]
}
```

---

#### Account statement

- **GET** `/api/transactions/statement`

Query params:

| Param | Description |
|-------|-------------|
| `account_id` | required |
| `from` | optional `YYYY-MM-DD` (inclusive) |
| `to` | optional `YYYY-MM-DD` (inclusive) |
| `limit` | default `100` |
| `offset` | default `0` |

Example: `GET /api/transactions/statement?account_id=17&from=2026-05-01&to=2026-05-31`

Response `200 OK`:

```json
{
  "items": [
    {
      "transaction_id": 120,
      "recorded_on": "2026-05-01T10:00:00Z",
      "description": "ATM Deposit",
      "transaction_type": "Deposit",
      "value_cents": 500000,
      "balance_after_cents": 500000
    },
    {
      "transaction_id": 121,
      "recorded_on": "2026-05-02T12:00:00Z",
      "description": "Payment | category: Food | merchant: Pizza Place",
      "transaction_type": "Payment",
      "value_cents": 7500,
      "balance_after_cents": 492500
    }
  ]
}
```

`value_cents` is signed relative to the statement account (+ incoming, − outgoing). `balance_after_cents` is the running balance after each entry.

---

#### User monthly summary

- **GET** `/api/transactions/summary/monthly?per_account_limit=500`

All totals reflect the **current calendar month (UTC)**. `per_account_limit` caps transactions loaded per account.

Response `200 OK`:

```json
{
  "total_incoming_cents": 1500000,
  "total_outgoing_cents": 800000,
  "total_volume_cents": 2300000,
  "per_type_totals": [
    { "transaction_type": "Deposit", "total_cents": 1500000 },
    { "transaction_type": "Payment", "total_cents": 300000 },
    { "transaction_type": "Send", "total_cents": 500000 }
  ],
  "daily_cumulative_spending": [
    {
      "date": "2026-05-01",
      "spending_cents": 30000,
      "cumulative_spending_cents": 30000
    },
    {
      "date": "2026-05-02",
      "spending_cents": 50000,
      "cumulative_spending_cents": 80000
    }
  ]
}
```

Internal transfers between the user's own accounts are deduplicated and net to zero in incoming/outgoing totals.

---

## Request lifecycle

```
Client → axum router → [require_auth on private routes]
       → handler: parse Path/Query/Json
       → service call
       → Ok(Json(...)) or ApiError (IntoResponse)
```

---

## Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `DATABASE_URL` | PostgreSQL connection string | local dev DSN in `main.rs` |
| `JWT_SECRET` | HMAC secret for JWT | dev default in `main.rs` |
| `RUST_LOG` | Tracing filter | `info` |

Server listens on `0.0.0.0:6767` (see `main.rs`).

---

## Notes for frontend

- Monetary amounts in JSON use `*_cents` (integers).
- Enums serialize as PascalCase strings (`Regular`, `RON`, `Deposit`, …).
- Timestamps are RFC3339 (`recorded_on`); dates are `YYYY-MM-DD` (`birth_date`).
- Always send `Authorization: Bearer <token>` on private routes.
