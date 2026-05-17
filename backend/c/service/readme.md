# Service Module

## Overview

The **service** module is the business logic layer of the application. It sits between the HTTP server and the database, enforcing all domain rules, validations, and transactional boundaries. Neither the server nor the database layer should contain business logic — that responsibility belongs exclusively here.

---

## Architecture
┌──────────────────────────────────────────────────┐ 
│ server layer │ │ (calls service functions)       │ 
└─────────────────────────┬────────────────────────┘
                          │
        ┌─────────────────▼─────────────────┐ 
        │ service layer                     │
        │    user_service account_service   │
        │    auth_service affiliate_service │ 
        │    transaction_service            │ 
        └─────────────────┬─────────────────┘ 
                          │ 
        ┌─────────────────▼─────────────────┐ 
        │ db layer                          │ 
        │ (repos: user, account, txn ...)   │ 
        └───────────────────────────────────┘

---

## File Reference

*(Files are expected to follow this naming convention.)*

### `user_service.h` / `user_service.c`
Manages user lifecycle.

- `user_service_create(user_t *input, user_t *out)` — validates and creates a new user. Enforces unique email, hashes password before storage.
- `user_service_get_by_id(int id, user_t *out)` — fetches a user, maps repo result to domain type.
- `user_service_update(int id, user_t *input)` — validates and applies updates.
- `user_service_delete(int id)` — soft-deletes or hard-deletes per policy.
- `user_service_list(user_t **out, int *count)` — returns paginated list.

### `auth_service.h` / `auth_service.c`
Handles authentication logic.

- `auth_service_login(const char *email, const char *password, char *out_token)` — verifies credentials, issues JWT on success.
- `auth_service_validate_token(const char *token, auth_claims_t *out)` — parses and validates a JWT, returns claims.
- `auth_service_logout(const char *token)` — invalidates session if applicable.
- Delegates password verification to a secure hashing utility.

### `account_service.h` / `account_service.c`
Manages financial accounts.

- `account_service_create(account_t *input, account_t *out)` — creates a new account tied to a user.
- `account_service_get(int id, account_t *out)`.
- `account_service_update_balance(int id, double delta)` — atomically adjusts balance; rejects if result would be negative.
- `account_service_close(int id)` — marks account as closed; rejects if balance non-zero.

### `transaction_service.h` / `transaction_service.c`
Orchestrates financial transactions — the most complex service.

- `transaction_service_transfer(int from_account, int to_account, double amount)`:
    - Validates both accounts exist and are active.
    - Checks sufficient balance on source.
    - Wraps debit + credit in a single DB transaction.
    - Records the transaction with timestamp, status, and reference.
    - Rolls back atomically on any failure.
- `transaction_service_get(int id, transaction_t *out)`.
- `transaction_service_list_by_account(int account_id, transaction_t **out, int *count)`.

### `affiliate_service.h` / `affiliate_service.c`
Manages affiliate relationships.

- `affiliate_service_register(affiliate_t *input, affiliate_t *out)` — creates an affiliate record.
- `affiliate_service_get(int id, affiliate_t *out)`.
- `affiliate_service_link_user(int affiliate_id, int user_id)` — associates a user with an affiliate.

---

## Error Handling

All service functions return an `int`:
- `0` — success.
- `SERVICE_ERR_NOT_FOUND` — the requested entity does not exist.
- `SERVICE_ERR_INVALID_INPUT` — validation failed; check the error detail.
- `SERVICE_ERR_CONFLICT` — uniqueness or state constraint violated.
- `SERVICE_ERR_INSUFFICIENT_FUNDS` — specific to financial operations.
- `SERVICE_ERR_INTERNAL` — unexpected failure; log and return 500 upstream.

Errors from the `db` layer are translated into service-layer error codes — the server never sees raw repo errors.

---


