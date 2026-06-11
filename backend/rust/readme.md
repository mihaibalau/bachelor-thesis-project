# Rust Backend — Architecture Overview

Gentlix banking API in Rust (Axum + SQLx). Same routes, JSON shapes, and PostgreSQL schema as `backend/c/` so both backends can run behind one load balancer.

**Layer docs:**

| Layer | Folder | Readme |
|-------|--------|--------|
| HTTP / JWT | `src/server/` | [src/server/readme.md](src/server/readme.md) |
| Use-cases | `src/service/` | [src/service/readme.md](src/service/readme.md) |
| SQL / SQLx | `src/db/` | [src/db/readme.md](src/db/readme.md) |
| Entities & validation | `src/domain/` | (inline modules — mirror of C `domain/`) |

C backend notes: [../c/readme.md](../c/readme.md)

---

## Layer stack

```
  Client (React)
       │  HTTP + JSON
       ▼
┌──────────────────────────────────────────┐
│  server/   axum + tower + serde          │
└──────────────────┬───────────────────────┘
                   │ ServiceError, DTOs
                   ▼
┌──────────────────────────────────────────┐
│  service/  UserService, TxService, …     │
└──────────────────┬───────────────────────┘
                   │ RepoError, domain types
                   ▼
┌──────────────────────────────────────────┐
│  db/       UserRepo, AccountRepo, …      │
└──────────────────┬───────────────────────┘
                   ▼
              PostgreSQL
```

Same inward dependency rule as C: routes don't call SQLx directly.

---

## Startup (`src/main.rs`)

1. `dotenvy` + `server::logging::init_tracing`
2. `Db::new` — SQLx pool (max 5 connections), cloned into repos
3. `Arc` repos → services → `Arc<AppState>` → `create_router` → `axum::serve`

---

## Repository traits (C vtable equivalent)

```rust
#[async_trait]
pub trait UserRepository: Send + Sync {
    async fn get_by_id(&self, user_id: UserId) -> Result<User, RepoError>;
}
```

`impl UserRepository for UserRepo` forwards to sqlx. Services are generic over traits; `main` fixes concrete types in `AppState`. See [src/service/readme.md](src/service/readme.md) for modules.

---

## Error flow

| Layer | Type | Role |
|-------|------|------|
| `domain/` | `DomainError` | Validation on entities / value objects |
| `db/` | `RepoError` | SQL / not found |
| `service/` | `ServiceError` | Business errors |
| `server/` | JSON envelope | `server/error.rs` |

---

## Build & config

```bash
cargo run --manifest-path backend/rust/Cargo.toml
```

| Variable | Description | Default |
|----------|-------------|---------|
| `DATABASE_URL` | SQLx Postgres URL | **required** |
| `JWT_SECRET` | HMAC secret (≥ 32 bytes) | **required** |
| `PORT` | HTTP listen port | `6767` |
| `CORS_ORIGIN` | Browser origin | `http://localhost:5173` |
| `RUST_LOG`, `LOG_FORMAT`, … | Tracing | [LOGGING.md](../LOGGING.md) |

Docker: `backend/rust/Dockerfile`.

---

## Differences from C (intentional)

| Topic | Rust | C |
|-------|------|---|
| HTTP | axum | libmicrohttpd |
| DB | connection pool | single PGconn |
| Polymorphism | `async_trait` | manual vtables |
| Login JWT | issued in service | signed in server after service login |

Use-case behaviour and API contract are kept aligned for the frontend and benchmarks.
