# HTTP API Overview

Base URL: `/api`  
All “private” endpoints require `Authorization: Bearer <JWT>` header (token from `POST /api/users/login`).

Errors share a common JSON shape:

```json
{
  "code": "validation_error | conflict | not_found | forbidden | repo_error | concurrency_error | domain_error | unexpected_error",
  "message": "Human readable message"
}
```

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
  "account_type": "Savings",   // "Savings" | "Credit" | "Regular"
  "currency": "RON",           // e.g. "RON" | "EUR" | "USD"
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
  "identifier_type": "tag",    // "tag" | "phone" (currently both use `tag` lookup)
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

If no compatible currencies between owner and target, returns `422 validation_error`.

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
    - `422 validation_error` if account does not exist.
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

---

## Notes for Frontend

- All timestamps/dates are strings (`YYYY-MM-DD` for `birth_date`).
- Monetary amounts are always in `*_cents` (integers) to avoid floating-point issues.
- Enums (`account_type`, `currency`) are serialized as PascalCase/upper-case strings as shown.
- For private routes, always send `Authorization: Bearer <token>` from `/api/users/login`.