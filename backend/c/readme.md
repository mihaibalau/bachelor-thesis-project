# C Backend — Architecture Overview

Gentlix banking API in C. Same routes, JSON shapes, and PostgreSQL schema as `backend/rust/` so both backends can run behind one load balancer.

**Layer docs (read these for depth):**

| Layer | Folder | Readme |
|-------|--------|--------|
| HTTP / JWT | `server/` | [server/readme.md](server/readme.md) |
| Use-cases | `service/` | [service/readme.md](service/readme.md) |
| SQL / libpq | `db/` | [db/readme.md](db/readme.md) |
| Entities & validation | `domain/` | [domain/readme.md](domain/readme.md) |
| Env, logging, parsing | `util/` | [util/readme.md](util/readme.md) |

---

## Layer stack

```
  Client (React)
       │  HTTP + JSON
       ▼
┌──────────────────────────────────────────┐
│  server/   libmicrohttpd + jansson       │  parse routes, auth, JSON
└──────────────────┬───────────────────────┘
                   │ ServiceError, DTOs
                   ▼
┌──────────────────────────────────────────┐
│  service/  UserService, TxService, …     │  business rules, vtable ports
└──────────────────┬───────────────────────┘
                   │ RepoError, domain types
                   ▼
┌──────────────────────────────────────────┐
│  db/       UserRepo, AccountRepo, …      │  SQL only (libpq)
└──────────────────┬───────────────────────┘
                   │
                   ▼
              PostgreSQL

  domain/  — shared types & invariants (used by db + service)
  util/    — dotenv, logging, safe string parsing (used everywhere)
```

Dependencies point inward: `server/` never calls libpq; `db/` never parses JSON; business rules live in `service/` + `domain/`.

---

## Startup (`main.c`)

1. Load `.env` (`util/dotenv`) and init logging (`util/log`).
2. `db_connect` — one `Db*` / one `PGconn` for the process.
3. Create concrete repos: `user_repo_new(db)`, `account_repo_new(db)`, …
4. Wire ports — wrap each repo in a service-facing `{ vtable, ctx }` (see below).
5. Construct services and `AppState` (service pointers + `JWT_SECRET`).
6. `http_server_start` — libmicrohttpd passes `AppState*` into every request callback.

The same `AccountRepo*` pointer is often shared across several services on one connection, so `BEGIN` on `user_repo` covers `account_repo` inserts in the same transaction.

---

## Manual vtables (Rust trait equivalent)

Rust uses `async_trait` repository traits. C has no traits, so each service defines a port:

```c
typedef struct UserRepository {
    const UserRepositoryVTable *vtable;
    void *ctx;                           /* concrete UserRepo* */
} UserRepository;
```

Adapters in `*_service.c` cast `ctx` and call the real repo. `main.c` bundles vtable + ctx via `user_repository_from_user_repo`, etc.

Call site:

```c
svc->user_repo.vtable->get_by_id(svc->user_repo.ctx, id, &user, &rerr);
```

Different services expose different vtables over the same `AccountRepo*` — same idea as different trait bounds in Rust, implemented by hand (thesis comparison).

---

## Error flow

| Layer | Type | Role |
|-------|------|------|
| `domain/` | `DomainError` | Invalid email, negative balance, bad IBAN |
| `db/` | `RepoError` | SQL failure, row not found |
| `service/` | `ServiceError` | Conflicts, auth business rules, mapped repo/domain errors |
| `server/` | JSON `{ status, code, message }` | HTTP mapping in `http_error.c` |

Translate upward; don't expose raw libpq messages to clients (use generic `repo_error` for 500).

---

## Request lifecycle

`MHD callback` → `http_router` → `http_*_dispatch` → `[http_require_auth]` → jansson parse → `*_service_*` → repos → JSON response. Details: [server/readme.md](server/readme.md).

---

## Concurrency

Heavy analytics (`transaction_service_compute_user_statistics`) use pthreads + atomics; Rust uses tokio + blocking threads for the same algorithms.

---

## Build & config

CMake target `c` → `c.exe`. Required env: `DATABASE_URL`, `JWT_SECRET`. Optional: `PORT` (6767), `CORS_ORIGIN`, logging vars — see [server/readme.md](server/readme.md#configuration).
