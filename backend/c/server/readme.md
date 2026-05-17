# Server Module

## Overview

The **server** module is the HTTP layer of the application. It is responsible for accepting incoming 
HTTP connections, routing requests to the appropriate handlers, managing authentication and authorization, 
and returning well-formed HTTP responses. It is intentionally decoupled from business logic — it delegates 
all domain operations to the **service** layer and only concerns itself with transport-level concerns.

---

## Architecture

                ┌──────────────────────────────┐
                │        http_server.c          │
                │  (lifecycle, bind, accept)    │
                └──────────────┬───────────────┘
                                │
                ┌──────────────▼───────────────┐
                │        http_router.c          │
                │  (URL dispatch, method match) │
                └──────┬───────────────┬────────┘
                        │               │
        ┌────────────────▼──┐     ┌──────▼─────────────────┐
        │   http_auth.c     │     │    http_users.c         │
        │ (login, JWT issue)│     │ (CRUD user endpoints)   │
        └────────────────┬──┘     └──────┬──────────────────┘
                        │               │
                ┌──────▼───────────────▼───────┐
                │        http_state.c           │
                │  (shared runtime state,       │
                │   DB handle, config refs)     │
                └──────────────────────────────┘


Every request flows through the **router**, which resolves the handler. Handlers receive a 
shared `http_state_t` object giving access to the database layer, configuration, and any other 
runtime dependencies. Authentication is enforced via JWT middleware before protected handlers are invoked.

---

## File Reference

### `http_server.h` / `http_server.c`
The entry point of the HTTP layer.

- Initialises the server socket (TCP, configurable port).
- Enters the accept loop and dispatches each connection to the router.
- Manages graceful shutdown (signal handling, resource cleanup).
- Does **not** parse HTTP itself — delegates to the router immediately after accepting.

### `http_router.h` / `http_router.c`
URL and method-based dispatch.

- Maintains a route table mapping `(METHOD, path-pattern)` → handler function.
- Supports path parameters (e.g. `/users/:id`).
- Returns `404 Not Found` for unmatched routes and `405 Method Not Allowed` for method mismatches.
- Calls JWT validation middleware before forwarding to protected routes.

### `http_auth.h` / `http_auth.c`
Authentication endpoints and JWT handling.

- Exposes `POST /auth/login` — validates credentials via the service layer, issues a signed JWT on success.
- Exposes `POST /auth/refresh` — validates a refresh token and issues a new access token.
- Exposes `POST /auth/logout` — invalidates the current session.
- Relies on `jwt_utils` for all cryptographic operations.

### `http_users.h` / `http_users.c`
User management endpoints.

- `GET    /users`       — list users (admin only).
- `GET    /users/:id`   — get a single user.
- `POST   /users`       — create a new user.
- `PUT    /users/:id`   — update an existing user.
- All write operations require a valid JWT with appropriate claims.

### `http_state.h` / `http_state.c`
Shared server state.

- Defines `http_state_t` — the context object passed to every handler.
- Holds references to the database handle, configuration values, and any caches.
- Constructed once at startup and read-only for the lifetime of a request.
- Thread-safe by design: mutable fields are protected by a mutex.

### `http_error.h` / `http_error.c`
Centralised HTTP error responses.

- Maps internal error codes (from the `domain` and `db` layers) to HTTP status codes.
- Serialises error payloads as JSON: `{ "error": "...", "code": ... }`.
- Provides helpers: `http_respond_400`, `http_respond_401`, `http_respond_403`, `http_respond_404`, `http_respond_500`.

### `jwt_utils.h` / `jwt_utils.c`
JWT creation and verification.

- Signs tokens using HMAC-SHA256 with a secret loaded from the environment.
- Verifies signature, expiry (`exp`), and issuer (`iss`) on every protected request.
- Exposes: `jwt_sign(payload, out_token)`, `jwt_verify(token, out_payload)`.
- Never stores secrets in memory longer than necessary.

---

## Request Lifecycle
Client  → accept() 
        → parse HTTP line + headers 
        → router: match route 
        → [if protected] jwt_utils: verify token 
        → handler: extract path/query params, parse JSON body 
        → service layer call 
        → http_error or http_respond_200 with JSON body 
        → write response 
        → close connection

---

## Error Handling

All handlers return an `int` status code:
- `0` — success, response already written.
- Negative values — an internal error; `http_error.c` writes the appropriate HTTP response.

The server **never** crashes on a bad client request. All parsing and service errors are 
caught and turned into HTTP error responses.

---

## Configuration

The server reads the following environment variables (via `.env`):

| Variable         | Description                              | Default  |
|------------------|------------------------------------------|----------|
| `SERVER_PORT`    | TCP port to listen on                    | `67676`  |
| `JWT_SECRET`     | HMAC secret for token signing            | *(none)* |
| `JWT_EXPIRY_SEC` | Access token lifetime in seconds         | `3600`   |

---

## Dependencies

| Layer    | Used for                                    |
|----------|---------------------------------------------|
| `service`| All business logic (validation, rules)      |
| `db`     | Never called directly — only via `service`  |
| `domain` | Request/response type definitions           |
| `util`   | String helpers, JSON serialisation          |