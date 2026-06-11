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
│   └── react/               # Vite + React + MUI user app — see frontend/react/README.md
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

Both backends expose the same **22 API routes** under `/api` (plus an unauthenticated `GET /health`) so the React app can talk to either one.

## API overview

| Area | Routes |
|------|--------|
| Health | `GET /health` (unauthenticated, outside `/api`) → `{"status":"ok"}` |
| Users | register, login, profile |
| Accounts | list, open, availability, detail |
| Affiliates | CRUD, resolve-target |
| Transactions | deposit, withdrawal, send, transfer, payment, recent, statement, summary, summary/monthly |
| Dashboard | aggregated balances, spending, recent activity |

Money-moving operations (deposit/withdrawal/send/transfer/payment) and user registration run inside a **single DB transaction** in both backends, so balance updates and the inserted row commit (or roll back) together.

## Environment variables

| Variable | Rust | C | Frontend |
|----------|------|---|----------|
| `DATABASE_URL` | required | required (`DB_CONN` alias) | — |
| `JWT_SECRET` | required (min 32 bytes) | required (min 32 bytes) | — |
| `CORS_ORIGIN` | optional (default `http://localhost:5173`) | optional (default `http://localhost:5173`) | — |
| `PORT` | optional (default `6767`) | optional (default `6767`) | — |
| `VITE_API_BASE_URL` | — | — | optional (default `http://localhost:6767/api`) |

Logging-related variables (`LOG_FORMAT`, `LOG_LEVEL`, `RUST_LOG`, `DEBUG_MODE`, `SERVICE_NAME`) are documented in [`backend/LOGGING.md`](backend/LOGGING.md). Copy each component's `.env.example` to `.env` (backends) / `.env.local` (frontend) and fill in real values — never commit real secrets.

## Deployment

See [`deploy/AWS.md`](deploy/AWS.md) for Docker images and an AWS deployment guide (ECR/ECS or a single docker-compose host, RDS, secrets, ports) plus a readiness checklist.

## Account rules

A user may hold **at most one account per `(account_type, currency)` pair** — e.g. one Regular RON, one Regular EUR, and one Savings RON can coexist, but not two Regular RON accounts. This is enforced in both backends by the service check (`exists_by_type_and_currency` in `open_account`) and backed by the migration `003` unique index on `(user_id, account_type, currency)`. A duplicate returns `409 conflict` with message `you already have a {type} {currency} account`, and `GET /api/accounts/availability` marks only the owned `(type, currency)` pairs unavailable.
