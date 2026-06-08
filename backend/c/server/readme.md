# C Server Layer Reference

The **server** layer is the HTTP/API boundary. It accepts requests via **libmicrohttpd**, enforces JWT auth on private routes, parses JSON, calls the **service** layer, and serializes responses. It contains **no business logic** — parsing, routing, auth extraction, and JSON mapping only.

Both backends (Rust and C) expose the same routes, request bodies, response shapes, and error codes so they can run behind one load balancer with one frontend and one database.

**Stack:** [GNU libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) (MHD) + [jansson](https://github.com/akheron/jansson) for JSON.

---

## Architecture

```
                ┌──────────────────────────────┐
                │           main.c             │
                │  (Db, repos, services,       │
                │   AppState, http_server)     │
                └──────────────┬───────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      http_server.c           │
                │  (MHD_start_daemon, bind)    │
                └──────────────┬───────────────┘
                               │
                ┌──────────────▼───────────────┐
                │      http_router.c           │
                │  prefix dispatch to:         │
                │  users / accounts /          │
                │  affiliates / transactions / │
                │  dashboard                   │
                └──────┬───────────┬───────────┘
                       │           │
       ┌───────────────▼──┐   ┌────▼────────────────────────────┐
       │  http_auth.c     │   │  http_{users,accounts,          │
       │  jwt_utils.c     │   │  affiliates,transactions}.c    │
       └──────────────────┘   └──────────────┬────────────────────┘
                                             │
                              ┌──────────────▼──────────────┐
                              │  http_util.c / http_error.c │
                              └──────────────┬──────────────┘
                                             │
                              ┌──────────────▼──────────────┐
                              │  service/ (use-cases)       │
                              └─────────────────────────────┘
```

Every request flows: **MHD callback** → `http_request_handler` → resource `*_dispatch` → optional `http_require_auth` → handler → service call → `http_send_json` / `http_send_service_error`.

---

## Module reference

Each file below lists public functions with purpose, behaviour, and what the handler delegates to the service layer. **No business rules** live here — only HTTP transport concerns.

---

### `http_server.h` / `http_server.c`

The entry point of the HTTP layer — wraps libmicrohttpd daemon lifecycle.

- `bool http_server_start(struct HttpServer *srv, AppState *state, unsigned short port);`
  - Purpose: Bind and start the MHD daemon.
  - Params: `srv` – server struct to fill; `state` – passed as `cls` to every callback; `port` – TCP port (`PORT` env in `main.c`, default 6767).
  - Behaviour: Calls `MHD_start_daemon` with `http_request_handler`, `MHD_USE_SELECT_INTERNALLY`, listening on `0.0.0.0`.
  - Returns: `true` on success, `false` if daemon failed to start.

- `void http_server_stop(struct HttpServer *srv);`
  - Purpose: Graceful shutdown.
  - Behaviour: `MHD_stop_daemon` if handle non-null.

---

### `http_router.h` / `http_router.c`

Top-level URL dispatch — mirrors axum `.nest("/api/…")`.

- `enum MHD_Result http_request_handler(void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls);`
  - Purpose: Single MHD access handler for all routes.
  - Behaviour:
    - Logs every non-`OPTIONS` request (`event=http_request method=… path=…`).
    - Uses `match_prefix` for path-boundary-safe matching (prevents `/api/accountsX` false positives).
    - Strips prefix and forwards **subpath** to the resource dispatcher.
    - Unmatched → `http_send_not_found`.
  - Dispatch table:
    - `/api/users` → `http_users_dispatch`
    - `/api/accounts` → `http_accounts_dispatch`
    - `/api/affiliates` → `http_affiliates_dispatch`
    - `/api/transactions` → `http_transactions_dispatch`
    - `/api/dashboard` → `http_dashboard_dispatch`

---

### `http_state.h` / `http_state.c`

Shared runtime dependency container.

- `typedef struct AppState { UserService *user_svc; AccountService *account_svc; TransactionService *tx_svc; AffiliateService *affiliate_svc; char jwt_secret[JWT_SECRET_MAX]; } AppState;`
  - Purpose: Same role as Rust `Arc<AppState>` — services + JWT secret.

- `void app_state_init(AppState *state, UserService *user_svc, AccountService *account_svc, TransactionService *tx_svc, AffiliateService *affiliate_svc, const char *jwt_secret);`
  - Purpose: Populate state at startup in `main.c` (copies secret into fixed buffer).

---

### `http_auth.h` / `http_auth.c`

JWT gate for private routes.

- `bool http_require_auth(struct MHD_Connection *conn, const AppState *state, AuthClaims *out_claims, ServiceError *err);`
  - Purpose: Extract and validate Bearer token before protected handlers run.
  - Behaviour:
    1. Read `Authorization` header from MHD connection.
    2. Require `Bearer ` prefix.
    3. Call `jwt_decode_user_id` with the full token (no fixed-size copy, so over-long tokens are rejected, never truncated).
    4. Fill `out_claims->sub` with the decoded user id.
  - Returns: `true` on success; `false` + `SERVICE_ERROR_VALIDATION` on missing header / bad scheme / invalid token.

---

### `jwt_utils.h` / `jwt_utils.c`

JWT creation and verification (HS256).

- `bool jwt_decode_user_id(const char *token, const char *secret, UserId *out_user_id, char *out_tag, size_t tag_cap);`
  - Purpose: Verify signature, expiry, and parse `sub` + `tag` claims.
  - Returns: `false` on any crypto or format failure.

- `bool jwt_encode_user_id(UserId user_id, const char *tag, const char *secret, char *out_token, size_t out_cap);`
  - Purpose: Issue token after successful login.
  - Behaviour: 24-hour expiry; same claim shape as Rust `Claims`.

---

### `http_error.h` / `http_error.c`

Maps service errors to HTTP responses.

- `typedef struct { int status; const char *code; const char *message; } ApiErrorBody;`

- `ApiErrorBody http_error_from_service_error(const ServiceError *err);`
  - Purpose: One-for-one mapping with Rust `ApiError::into_response`.
  - Behaviour: Sets `status`, `code` string, copies `message` from `ServiceError`.

---

### `http_util.h` / `http_util.c`

Shared response and body-buffer helpers.

- `BodyBuffer` – `{ char *data; size_t len; size_t cap; }` for accumulating POST/PATCH bodies across MHD upload callbacks.

- `void body_buffer_free(BodyBuffer *bb);`
- `bool body_buffer_append(BodyBuffer *bb, const char *data, size_t size);`
  - Purpose: Growable buffer; returns `false` on OOM.

- `void add_cors_headers(struct MHD_Response *res);`
  - Purpose: Add CORS headers to every response (mirrors the Rust `CorsLayer`).
  - Behaviour: `Access-Control-Allow-Origin` = `CORS_ORIGIN` env (default `http://localhost:5173`); allows headers `Content-Type, Authorization`; allows methods `GET, POST, PUT, PATCH, DELETE, OPTIONS`; `Access-Control-Max-Age: 86400`.

- `enum MHD_Result http_send_json(struct MHD_Connection *conn, int status, const char *json);`
  - Purpose: Respond with `Content-Type: application/json`.

- `enum MHD_Result http_send_empty(struct MHD_Connection *conn, int status);`
  - Purpose: 200 responses with no body (affiliate create/delete/patch).

- `enum MHD_Result http_send_service_error(struct MHD_Connection *conn, const ServiceError *err);`
  - Purpose: Serialize `{ status, code, message }` JSON error envelope.

- `enum MHD_Result http_send_not_found(struct MHD_Connection *conn);`
  - Purpose: 404 for unknown routes.

---

### `http_users.h` / `http_users.c`

User registration, login, profile.

- `enum MHD_Result http_users_dispatch(AppState *state, struct MHD_Connection *conn, const char *subpath, const char *method, const char *upload_data, size_t *upload_data_size, void **con_cls);`
  - Routes:
    - `POST /` (empty subpath) → register — public; parses JSON; calls `user_service_register_user`.
    - `POST /login` → login — public; calls `user_service_login_user` then `jwt_encode_user_id`.
    - `GET /{id}` → profile — private via `http_require_auth`; rejects if `id != token sub`; calls `user_service_get_user_with_accounts`.

- `enum MHD_Result http_users_get_user_with_accounts(...)` – builds JSON from `UserWithAccounts`.
- `enum MHD_Result http_users_not_found(...)` – legacy 404 helper.

---

### `http_accounts.h` / `http_accounts.c`

Account open, get, availability.

- `enum MHD_Result http_accounts_dispatch(...);`
  - All routes require auth.
  - **Route order:** literal `"/availability"` matched before numeric `/{id}` parse.
  - `POST /` → `account_service_open_account_raw`; reload account for JSON response.
  - `GET /availability` → `account_service_get_account_availability`; shape matrix into nested JSON.
  - `GET /{id}` → parse id with `strtoll` (400 on malformed); ownership check; `account_service_get_account`.
  - Malformed path id → **400** `bad_request` (matches axum `Path<i64>` rejection).

---

### `http_affiliates.h` / `http_affiliates.c`

Affiliate CRUD, list, resolve-target.

- `enum MHD_Result http_affiliates_dispatch(...);`
  - `GET /` → parse query params into `ListAffiliatesParams`; `list_affiliates_view`.
  - `POST /` → create affiliate.
  - `POST /resolve-target` → `identifier_type` + `identifier`; tag → `resolve_target_by_tag`; phone → validation error.
  - `GET/PATCH/DELETE /{sub_account_id}` → get view / rename / delete.

---

### `http_transactions.h` / `http_transactions.c`

All money movement and analytics endpoints.

- `enum MHD_Result http_transactions_dispatch(...);`
  - POST handlers parse JSON and call matching `transaction_service_record_*_for_user`; return bare integer transaction id JSON.
  - `GET /recent` → query `account_id`, optional `limit`.
  - `GET /statement` → query params forwarded to `compute_account_statement_for_user_from_strings`.
  - `GET /summary/monthly` → compute UTC month bounds in handler (agreed exception); call `compute_user_statistics` with window; build cumulative daily spending array.

---

## Request lifecycle

```
Client → MHD daemon (http_request_handler)
       → resource *_dispatch (BodyBuffer for POST/PATCH body chunks)
       → [http_require_auth on private routes]
       → jansson parse → service *_service_* call
       → http_send_json / http_send_service_error / http_send_empty
       → add_cors_headers on every response
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
| `for_send_currency` | str | keep only affiliates whose recipient account currency matches (used by the Send flow) |
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

#### Transaction summary (filtered)

- **GET** `/api/transactions/summary?from=YYYY-MM-DD&to=YYYY-MM-DD&account_id=&transaction_type=&per_account_limit=500`

Filtered aggregates for the Statistics page (totals, per-type, daily net, cumulative spending, payment categories, balances). Implemented in `http_transactions.c` → `transaction_service_compute_user_statistics_extended`.

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

#### Dashboard

- **GET** `/api/dashboard`

Aggregated home-screen data. Implemented in `http_dashboard.c`.

---

## Request lifecycle

```
Client → MHD daemon → http_request_handler (prefix match)
       → *_dispatch (body buffering via BodyBuffer on POST/PATCH)
       → [http_require_auth on private routes]
       → parse JSON with jansson → service call
       → http_send_json / http_send_service_error / http_send_empty
```

MHD may invoke the callback multiple times per request while uploading a body; `BodyBuffer` accumulates chunks until `*upload_data_size == 0`.

---

## Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `DATABASE_URL` | libpq connection string | **required** (`DB_CONN` alias supported) |
| `JWT_SECRET` | HMAC secret for JWT | **required** (max 255 bytes) |
| `CORS_ORIGIN` | Allowed browser origin | `http://localhost:5173` |
| `PORT` | HTTP listen port | `6767` |

`.env` is loaded from the current directory, then `../.env`, then next to the executable (`load_dotenv_*` in `main.c`); environment variables set by the container/host take precedence. Logging variables (`LOG_FORMAT`, `LOG_LEVEL`, `DEBUG_MODE`, `SERVICE_NAME`) are documented in [`backend/LOGGING.md`](../../LOGGING.md).

Server binds `0.0.0.0:$PORT` (default 6767). When run interactively it prints `Press ENTER to stop...` and shuts down on input.

---

## Notes for frontend

- Monetary amounts in JSON use `*_cents` (integers).
- Enums serialize as PascalCase strings (`Regular`, `RON`, `Deposit`, …).
- Timestamps are RFC3339 (`recorded_on`); dates are `YYYY-MM-DD` (`birth_date`).
- Always send `Authorization: Bearer <token>` on private routes.
- CORS headers are added on every response (`add_cors_headers`).

---

## Dependencies

| Layer | Used for |
|-------|----------|
| `service/` | All business logic |
| `domain/` | Value types referenced in JSON mapping |
| libmicrohttpd | HTTP server |
| jansson | JSON parse/serialize |
| libpq (via `db/`) | Never called directly from server code |
