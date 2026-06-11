# AWS deployment guide

This document describes how to run Gentlix Bank in containers and deploy it on AWS.
The application is a React frontend, PostgreSQL, and **one of two interchangeable backends**
(Rust or C) that expose the same 22-route API on port **6767**.

Run only one backend at a time — both bind the same port.

## What is in the repository

| Artifact | Path | Role |
|----------|------|------|
| Rust backend image | `backend/rust/Dockerfile` | Multi-stage Cargo build, `debian-slim` runtime |
| C backend image | `backend/c/Dockerfile` | Debian build (CMake, libpq, MHD, jansson, OpenSSL, argon2), slim runtime |
| Frontend image | `frontend/react/Dockerfile` + `nginx.conf` | Vite build served by nginx |
| Local stack | `infra/docker-compose.yml` | Postgres, migration runner, chosen backend, frontend |
| Compose env template | `infra/.env.example` | Postgres credentials, `JWT_SECRET`, `CORS_ORIGIN`, and related settings |
| Migrations | `db/migrations/001..003` | Schema; apply in order |
| Seeds | `db/seeds/` | Optional demo data and 100k-row benchmark set |

Each component has a `.dockerignore` that keeps `.env`, build artefacts, and `node_modules`
out of the image. Secrets belong in the runtime environment, not in the build context.

## Environment variables

| Variable | Rust | C | Frontend | Notes |
|----------|------|---|----------|-------|
| `DATABASE_URL` | required | required (`DB_CONN` alias) | — | e.g. `postgres://user:pass@host:5432/gentlix_bank` |
| `JWT_SECRET` | required | required (max 255 bytes) | — | long random value; rotate before any public deploy |
| `CORS_ORIGIN` | optional | optional | — | exact browser origin; default `http://localhost:5173` |
| `PORT` | optional | optional | — | default `6767` |
| `LOG_FORMAT` | optional | optional | — | set `json` for CloudWatch Logs Insights |
| `RUST_LOG` | optional | — | — | tracing filter; default `info` |
| `LOG_LEVEL` | — | optional | — | `trace` / `debug` / `info` / `warn` / `error` |
| `DEBUG_MODE` | optional | optional | — | `1` or `true` for verbose logs |
| `SERVICE_NAME` | optional | optional | — | label in logs; defaults `gentlix-rust` / `gentlix-c` |
| `VITE_API_BASE_URL` | — | — | build arg | baked at **build time**; default `http://localhost:6767/api` |

Logging details for both backends: [`../backend/LOGGING.md`](../backend/LOGGING.md).

## Local run or single EC2 host (docker-compose)

For a single machine — your laptop or one EC2 instance — compose is enough.

```sh
cp infra/.env.example infra/.env        # set POSTGRES_PASSWORD, JWT_SECRET, CORS_ORIGIN

docker compose -f infra/docker-compose.yml up -d db
docker compose -f infra/docker-compose.yml --profile migrate run --rm migrate

docker compose -f infra/docker-compose.yml --profile rust up -d --build
# or:  --profile c
```

Frontend: `http://<host>:5173`. API: `http://<host>:6767/api`.

On a host reachable from the browser, `CORS_ORIGIN` and the frontend build arg
`VITE_API_BASE_URL` must match the public URLs you actually use.

## AWS: ECR and ECS/Fargate

The typical production layout is RDS for Postgres, ECR for images, and Fargate tasks
behind an ALB. The steps below assume that pattern; adjust regions and names to your account.

**1. RDS (PostgreSQL 16)**  
Create the instance and note endpoint, user, password, and database name. Build
`DATABASE_URL` from those values. Store `DATABASE_URL` and `JWT_SECRET` in Secrets Manager
(or SSM Parameter Store) — not as plaintext in the task definition.

**2. Migrations**  
Apply them to RDS before the first backend start:

```sh
psql "$DATABASE_URL" -f db/migrations/001_init.sql
psql "$DATABASE_URL" -f db/migrations/002_transaction_type_payment.sql
psql "$DATABASE_URL" -f db/migrations/003_account_per_type_currency.sql
```

Alternatively, run the compose `migrate` service or `db/migrate.sh` from a bastion that
can reach RDS.

**3. Build and push images to ECR**

```sh
aws ecr get-login-password --region <region> | docker login --username AWS --password-stdin <acct>.dkr.ecr.<region>.amazonaws.com

docker build -t gentlix-rust ./backend/rust
# or build backend/c instead — same API, pick one for the deployment

docker build -t gentlix-frontend \
  --build-arg VITE_API_BASE_URL=https://api.example.com/api \
  ./frontend/react

docker tag gentlix-rust <acct>.dkr.ecr.<region>.amazonaws.com/gentlix-rust:latest
docker push <acct>.dkr.ecr.<region>.amazonaws.com/gentlix-rust:latest
# repeat tag/push for the frontend image
```

**4. Task definitions**

- **Backend:** container port 6767; inject `DATABASE_URL` and `JWT_SECRET` from Secrets Manager;
  set `LOG_FORMAT=json`, `SERVICE_NAME`, and `CORS_ORIGIN`; use the `awslogs` driver for CloudWatch.
- **Frontend (nginx):** container port 80.

**5. Networking**

- ALB on :443 (TLS certificate from ACM) → frontend target group on :80.
- Path or host rule (or a second ALB) → backend target group on :6767 for `/api/*`.
- RDS security group: allow :5432 only from the backend service security group.

**6. CORS and frontend URL**

Set backend `CORS_ORIGIN` to the frontend's public origin (scheme + host + port if non-default).
Rebuild the frontend with `VITE_API_BASE_URL` pointing at the backend's public `/api` base URL.
Both must stay in sync after any domain change.

## Caveats (read before you deploy)

**Rust image and SQLx offline cache.**  
The Rust DB layer uses `sqlx::query!` / `query_as!` macros checked against a live database
at compile time. There is no committed `.sqlx/` cache yet, so a Docker build with no database
reachable will fail. Before building the Rust image:

```sh
cd backend/rust
cargo install sqlx-cli --no-default-features --features postgres
DATABASE_URL=postgres://user:pass@localhost:5432/gentlix_bank cargo sqlx prepare
# commit the generated .sqlx/ directory
```

The Dockerfile sets `SQLX_OFFLINE=true`. The other option is to build with a reachable
`DATABASE_URL` instead of committing the cache.

**C backend shutdown on Fargate.**  
`backend/c/main.c` waits on `getchar()` and exits on EOF. A headless ECS/Fargate task has no
stdin, so the process can exit right after startup. Compose works around this with
`stdin_open` and `tty` on the `backend-c` service. For ECS, the proper fix is to block on
`SIGTERM` / `SIGINT` instead of `getchar()` — that change belongs in the C backend, not here.

**Local `.env` files.**  
`.env` is listed in `.gitignore` for both backends. Do not include real `backend/c/.env` or
`backend/rust/.env` files in a thesis archive or any public upload. Use `.env.example` and
runtime secrets (Secrets Manager, compose env, task definition) only. If those files were
ever committed in an older revision, rotate `JWT_SECRET` and the database password before deploy.

**C `setenv` precedence on Linux.**  
`dotenv.c` overwrites existing environment variables. A stray `.env` inside the image could
override injected secrets. `.dockerignore` excludes `.env`, so container env vars remain
authoritative — keep it that way.

## Readiness checklist

- [ ] Run `cargo sqlx prepare` and commit `backend/rust/.sqlx/` (required for Rust Docker build).
- [ ] Choose a C shutdown strategy for ECS (signal handler) or run with a TTY only for experiments.
- [ ] Confirm no real `.env` files are in the upload; use `.env.example` templates only.
- [ ] Provision RDS Postgres 16; build `DATABASE_URL`; store it and `JWT_SECRET` in Secrets Manager.
- [ ] Apply migrations `001` → `002` → `003` on RDS (plus any seeds you need).
- [ ] `docker build` and `docker push` for the chosen backend and the frontend image to ECR.
- [ ] Build the frontend with production `VITE_API_BASE_URL`.
- [ ] Create ECS task definitions (ports 6767 / 80), `awslogs` driver, `LOG_FORMAT=json`.
- [ ] ALB + ACM TLS; route `/api/*` to backend:6767, other paths to frontend:80.
- [ ] Security groups: ALB → services; backend → RDS:5432 only.
- [ ] Set `CORS_ORIGIN` to the public frontend origin; verify login end-to-end.
- [ ] (Optional) add a dedicated health route for ALB checks — today you may need to point
      health checks at an existing route that returns a stable status code.
