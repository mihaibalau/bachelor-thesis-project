# HTTP API Overview

Base URL: `/api`  
All “private” endpoints require `Authorization: Bearer <JWT>` header (token from `POST /api/users/login`).

Errors share a common JSON shape and consistent HTTP status codes:

```json
{
  "status": 400,
  "code": "validation_error",
  "message": "Human readable message"
}
```

HTTP mapping:
- 400 BAD_REQUEST → code: validation_error | domain_error
- 403 FORBIDDEN → code: forbidden
- 404 NOT_FOUND → code: not_found (also used when a resource exists but is not owned by the caller to avoid leaking existence)
- 409 CONFLICT → code: conflict | concurrency_error
- 500 INTERNAL_SERVER_ERROR → code: repo_error | unexpected_error

---

## Users & Auth (`/api/users`)

### Register user

- **POST** `/api/users` (public)
- Body:

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

- Success `200 OK`:

```json
{
  "user_id": 1
}
```

---

### Login

- **POST** `/api/users/login` (public)
- Body:

```json
{
  "email": "mihai@example.com",
  "password": "plain-password"
}
```

- Success `200 OK`:

```json
{
  "token": "jwt-token-here",
  "user_id": 1
}
```

---

### Get user with accounts

- **GET** `/api/users/{id}` (private, must be same user as in token)

- Success `200 OK`:

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
      "balance_cents": 0,
      "iban": "RO49AAAA1B31007593840000"
    }
  ]
}
```

---

## Accounts (`/api/accounts`)

### Open account

- **POST** `/api/accounts` (private)
- Body:

```json
{
  "account_type": "Savings",
  "currency": "RON",
  "initial_balance_cents": 0
}
```

- Success `200 OK` – example response (if you return the created account):

```json
{
  "id": 42,
  "account_type": "Savings",
  "currency": "RON",
  "balance_cents": 0,
  "iban": "RO54AAAA1B31007593840001"
}
```

If user already has that `(account_type, currency)` (depending on your rule), service responds with `409 Conflict`:

```json
{
  "code": "conflict",
  "message": "account conflict: user 1 already has an account type of Savings"
}
```

---

### Get account by id

- **GET** `/api/accounts/{id}` (private, must own the account)

- Success `200 OK`:

```json
{
  "id": 42,
  "account_type": "Savings",
  "currency": "RON",
  "balance_cents": 0,
  "iban": "RO54AAAA1B31007593840001"
}
```

---

### Get account availability (for UI)

- **GET** `/api/accounts/availability` (private)

Returns, for each account type, which currencies are still available for the current user:

```json
{
  "types": [
    {
      "account_type": "Savings",
      "has_any_available": true,
      "currencies": [
        { "currency": "RON", "available": false },
        { "currency": "EUR", "available": true },
        { "currency": "USD", "available": true }
      ]
    },
    {
      "account_type": "Credit",
      "has_any_available": false,
      "currencies": [
        { "currency": "RON", "available": false },
        { "currency": "EUR", "available": false },
        { "currency": "USD", "available": false }
      ]
    }
  ]
}
```

---

## Affiliates (`/api/affiliates`)

### Resolve affiliate target (tag/phone → currencies)

Used by FE to power the “add by tag/phone + pick currency” flow.

- **POST** `/api/affiliates/resolve-target` (private)
- Body:

```json
{
  "identifier_type": "tag",
  "identifier": "mihai123"
}
```

- Success `200 OK`:

```json
{
  "recipient_user_id": 2,
  "recipient_full_name": "John Doe",
  "currencies": [
    {
      "currency": "RON",
      "recipient_sub_account_id": 77
    },
    {
      "currency": "EUR",
      "recipient_sub_account_id": 78
    }
  ]
}
```

If no compatible currencies between owner and target, returns `400 validation_error`.

---

### Create affiliate

- **POST** `/api/affiliates` (private)
- Body:

```json
{
  "recipient_sub_account_id": 77,
  "nickname": "John salary"
}
```

- Success: `200 OK` with empty body (`{}` if you choose to serialize unit).

- Errors:
    - `400 validation_error` if account does not exist.
    - `409 conflict` if affiliate already exists for that `(owner, sub_account)` pair.

---

### Get single affiliate

- **GET** `/api/affiliates/{sub_account_id}` (private – must own the affiliate)

- Success `200 OK`:

```json
{
  "recipient_sub_account_id": 77,
  "nickname": "John salary",
  "recipient_full_name": "John Doe",
  "currency": "RON"
}
```

---

### Update nickname

- **PATCH** `/api/affiliates/{sub_account_id}` (private)
- Body:

```json
{
  "nickname": "John – rent"
}
```

- Success: `200 OK` empty body.

If no such affiliate for current user → `404 not_found`.

---

### Delete affiliate

- **DELETE** `/api/affiliates/{sub_account_id}` (private)

- Success: `200 OK` empty body.
- If not found → `404 not_found`.

---

### List affiliates (pagination, search, filter, sort)

- **GET** `/api/affiliates` (private)
- Query params (all optional):

| Param       | Type | Description                                                 |
|------------|------|-------------------------------------------------------------|
| `page`     | u32  | 1-based, default 1                                          |
| `page_size`| u32  | default 20, clamped [1, 100]                                |
| `search`   | str  | substring on nickname or recipient_full_name; min 2 chars   |
| `currency` | str  | filter by currency (e.g. `RON`, `EUR`)                      |
| `sort`     | str  | `"asc"` (default) or `"desc"` by nickname (case-insensitive)|

- Example request:

`GET /api/affiliates?page=1&page_size=10&search=jo&currency=RON&sort=asc`

- Success `200 OK`:

```json
{
  "items": [
    {
      "recipient_sub_account_id": 77,
      "nickname": "John salary",
      "recipient_full_name": "John Doe",
      "currency": "RON"
    },
    {
      "recipient_sub_account_id": 80,
      "nickname": "John savings",
      "recipient_full_name": "John Doe",
      "currency": "RON"
    }
  ],
  "page": 1,
  "page_size": 10,
  "total": 2
}
```

Search is ignored if `search` length < 2 characters.

## Transactions (`/api/transactions`)

All transaction endpoints are private and require `Authorization: Bearer <JWT>`.

Supported `transaction_type` values:

- `"Deposit"`
- `"Withdrawal"`
- `"Send"`
- `"Transfer"`
- `"Payment"`

---

### Record deposit (ATM Deposit)

Simulates depositing cash at an ATM: money moves from the bank’s main account (ID `1`) to the user’s account.

- **POST** `/api/transactions/deposit`

- Body:

```json
{
  "account_id": 17,
  "amount": 100
}
```

`amount` is without cents (e.g. `100` → `10000` cents).

- Success `200 OK`:

```json
{
  "id": 123
}
```

---

### Record withdrawal (ATM Withdrawal)

Simulates withdrawing cash: money moves from the user account to the bank’s main account (ID `1`).

- **POST** `/api/transactions/withdrawal`

- Body:

```json
{
  "account_id": 17,
  "amount": 50
}
```

- Success `200 OK`:

```json
{
  "id": 124
}
```

---

### Record send (user → other user)

Uses two concrete account IDs (de obicei obținute din affiliates / resolve-target).  
Validates same currency; otherwise returns validation error (“use exchange”).[file:23]

- **POST** `/api/transactions/send`

- Body:

```json
{
  "from_account_id": 17,
  "recipient_account_id": 77,
  "value_cents": 250000,
  "message": "Rent for May"
}
```

- Success `200 OK`:

```json
{
  "id": 125
}
```

---

### Record transfer (between own accounts)

Transfers between conturi ale aceluiași user (e.g. Regular → Savings).  
Momentan doar same-currency; cross-currency va fi prin exchange ulterior.[file:23]

- **POST** `/api/transactions/transfer`

- Body:

```json
{
  "from_account_id": 17,
  "to_account_id": 18,
  "value_cents": 100000
}
```

- Success `200 OK`:

```json
{
  "id": 126
}
```

---

### Record payment (card-like merchant payment)

Simulează o plată la comerciant: bani pleacă din contul user-ului în contul „băncii/merchant aggregate” (ID `1`).  
Description include `category`, `merchant_name` și un `note` opțional.[file:23]

- **POST** `/api/transactions/payment`

- Body:

```json
{
  "from_account_id": 17,
  "amount": 42,
  "category": "Food",
  "merchant_name": "McBurger",
  "note": "Lunch menu"
}
```

- Success `200 OK`:

```json
{
  "id": 127
}
```

---

### Recent transactions (for dashboard widget)

Returns the most recent transactions for one account, ordered latest first.[file:24]

- **GET** `/api/transactions/recent?account_id=17&limit=5`

- Success `200 OK`:

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
    },
    {
      "id": 126,
      "from_account_id": 17,
      "to_account_id": 18,
      "transaction_type": "Transfer",
      "value_cents": 100000,
      "recorded_on": "2026-05-18T14:10:00Z",
      "description": "Transfer Regular RON -> Savings RON"
    }
  ]
}
```

---

### Account statement (running balance)

Returns a chronological statement for one account with running balance after each transaction — perfect for per-account graphs.[file:23][file:24]

- **GET** `/api/transactions/statement`

Query params:

| Param      | Type   | Description                          |
|-----------|--------|--------------------------------------|
| `account_id` | i64 | required                             |
| `from`    | str    | optional, `YYYY-MM-DD` (inclusive)   |
| `to`      | str    | optional, `YYYY-MM-DD` (inclusive)   |
| `limit`   | i64    | optional, default `100`              |
| `offset`  | i64    | optional, default `0`                |

- Example:

`GET /api/transactions/statement?account_id=17&from=2026-05-01&to=2026-05-31`

- Success `200 OK`:

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

---

### User monthly summary + cumulative spending

Aggregated stats across **all** accounts of current user, plus data ready for charts (daily spending & cumulative spending for current month).  
Backed by `compute_user_statistics` + filtering on current month.[file:23]

- **GET** `/api/transactions/summary/monthly?per_account_limit=500`

`per_account_limit` = max number of transactions loaded per account (for performance tuning).

- Success `200 OK`:

```json
{
  "total_incoming_cents": 1500000,
  "total_outgoing_cents": 800000,
  "total_volume_cents": 2300000,
  "per_type_totals": [
    { "transaction_type": "Deposit", "total_cents": 1500000 },
    { "transaction_type": "Payment", "total_cents": -300000 },
    { "transaction_type": "Send", "total_cents": -500000 }
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

---

## Notes for Frontend

- All timestamps/dates are strings (`YYYY-MM-DD` for `birth_date`).
- Monetary amounts are always in `*_cents` (integers) to avoid floating-point issues.
- Enums (`account_type`, `currency`) are serialized as PascalCase/upper-case strings as shown.
- For private routes, always send `Authorization: Bearer <token>` from `/api/users/login`.

