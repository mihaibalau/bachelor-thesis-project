# Gentlix Bank

Bachelor thesis project: dual backends (Rust + C), React frontend, PostgreSQL.

```
bachelor-thesis-project/
├── db/
│   ├── migrations/          # SQL migrations (run 001 → 002 → 003 in order)
│   ├── seeds/               # Optional demo + benchmark data + run_benchmark.sh
│   └── migrate.sh           # Helper script to apply all migrations
├── backend/
│   ├── rust/                # Axum REST API (port 6767)
│   ├── c/                   # libmicrohttpd REST API (port 6767, same routes)
│   └── LOGGING.md           # Shared logging contract (both backends)
├── frontend/
│   └── react/               # Vite + React + MUI user app (port 5173)
├── infra/
│   └── docker-compose.yml   # PostgreSQL + optional backend/frontend images
└── deploy/
    └── AWS.md               # AWS deployment guide + readiness checklist
```

## Quick start

1. Start Postgres: `docker compose -f infra/docker-compose.yml up -d`
2. Run migrations (in order):
   ```sh
   psql -U USER -d gentlix_bank -f db/migrations/001_init.sql
   psql -U USER -d gentlix_bank -f db/migrations/002_transaction_type_payment.sql
   psql -U USER -d gentlix_bank -f db/migrations/003_account_per_type_currency.sql
   ```
   Or: `sh db/migrate.sh -U USER -d gentlix_bank`
3. Optional seeds:
   - Dashboard demo (after registering `mihaiblu@gmail.com`): `psql -U USER -d gentlix_bank -f db/seeds/dashboard_demo.sql`
   - **Benchmark stress test**: `gentlix@benchmark.com` / `admin` — see `db/seeds/BENCHMARK.md`
     - Benchmark **100k transactions**: `db/seeds/benchmark_seed.sql` (see `db/seeds/BENCHMARK.md`)
4. Copy `.env.example` → `.env` in `backend/rust` and/or `backend/c` (both need `DATABASE_URL` and `JWT_SECRET`)
5. Start **one** backend (both bind port **6767**):
   - Rust: `cd backend/rust && cargo run`
   - C: build with CMake in `backend/c`, run from `backend/c` (so `.env` is found)
6. Frontend: `cd frontend/react && npm install && npm run dev`

Both backends expose the same **22 API routes** so the React app can talk to either one.

## API overview

| Area | Routes |
|------|--------|
| Users | register, login, profile |
| Accounts | list, open, availability, detail |
| Affiliates | CRUD, resolve-target |
| Transactions | deposit, withdrawal, send, transfer, payment, recent, statement, summary, summary/monthly |
| Dashboard | aggregated balances, spending, recent activity |

## Environment variables

| Variable | Rust | C | Frontend |
|----------|------|---|----------|
| `DATABASE_URL` | required | required (`DB_CONN` alias) | — |
| `JWT_SECRET` | required | required | — |
| `CORS_ORIGIN` | optional (default `http://localhost:5173`) | optional (default `http://localhost:5173`) | — |
| `PORT` | optional (default `6767`) | optional (default `6767`) | — |
| `VITE_API_BASE_URL` | — | — | optional (default `http://localhost:6767/api`) |

Logging-related variables (`LOG_FORMAT`, `LOG_LEVEL`, `RUST_LOG`, `DEBUG_MODE`, `SERVICE_NAME`) are documented in [`backend/LOGGING.md`](backend/LOGGING.md). Copy each component's `.env.example` to `.env` (backends) / `.env.local` (frontend) and fill in real values — never commit real secrets.

## Deployment

See [`deploy/AWS.md`](deploy/AWS.md) for Docker images and an AWS deployment guide (ECR/ECS or a single docker-compose host, RDS, secrets, ports) plus a readiness checklist.

## Account rules

The service layer enforces **one account per account type** per user (regardless of currency): once a user owns any `Regular` account, they cannot open another `Regular` account, and `GET /api/accounts/availability` marks the whole type as unavailable. (Migration `003` adds a `(user_id, account_type, currency)` unique index, which is more permissive than this service rule; the service check in `open_account` is the effective constraint.)
