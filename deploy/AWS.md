# AWS deployment guide

How to containerize and deploy Gentlix Bank (one backend + frontend + PostgreSQL).
The stack is **two interchangeable backends** (Rust and C) implementing the same
22-route API on port **6767**, a static React frontend, and PostgreSQL.

> Run **one** backend at a time — both bind port 6767.

---

## 1. What exists in the repo

| Artifact | Path | Purpose |
|----------|------|---------|
| Rust backend image | `backend/rust/Dockerfile` | Multi-stage Cargo build → `debian-slim` runtime |
| C backend image | `backend/c/Dockerfile` | Debian build (CMake + libpq/MHD/jansson/OpenSSL/argon2) → slim runtime |
| Frontend image | `frontend/react/Dockerfile` + `nginx.conf` | Vite build → nginx static serving |
| Local/single-host stack | `infra/docker-compose.yml` | Postgres + migrate runner + chosen backend + frontend |
| Compose env template | `infra/.env.example` | Postgres creds, `JWT_SECRET`, `CORS_ORIGIN`, etc. |
| Migrations | `db/migrations/001..003` | Schema (apply in order) |
| Seeds | `db/seeds/` | Optional demo + 100k benchmark data |

`.dockerignore` files exclude `.env`, build outputs, and `node_modules` from every
image so **no secrets are baked in** — all secrets come from the runtime environment.

---

## 2. Environment variables (per component)

| Variable | Rust | C | Frontend | Notes |
|----------|------|---|----------|-------|
| `DATABASE_URL` | required | required (`DB_CONN` alias) | — | `postgres://user:pass@host:5432/gentlix_bank` |
| `JWT_SECRET` | required | required (max 255 bytes) | — | long random secret; **rotate before deploy** |
| `CORS_ORIGIN` | optional | optional | — | exact browser origin; default `http://localhost:5173` |
| `PORT` | optional | optional | — | default `6767` |
| `LOG_FORMAT` | optional | optional | — | `json` for CloudWatch Logs Insights |
| `RUST_LOG` | optional | — | — | tracing filter, default `info` |
| `LOG_LEVEL` | — | optional | — | `trace`/`debug`/`info`/`warn`/`error` |
| `DEBUG_MODE` | optional | optional | — | `1`/`true` for verbose logs |
| `SERVICE_NAME` | optional | optional | — | appears in logs; defaults `gentlix-rust` / `gentlix-c` |
| `VITE_API_BASE_URL` | — | — | build arg | **baked at build time**; default `http://localhost:6767/api` |

Full logging contract: [`../backend/LOGGING.md`](../backend/LOGGING.md).

---

## 3. Local / single EC2 host (docker-compose)

```sh
cp infra/.env.example infra/.env        # edit POSTGRES_PASSWORD, JWT_SECRET, CORS_ORIGIN

# Start Postgres and apply migrations
docker compose -f infra/docker-compose.yml up -d db
docker compose -f infra/docker-compose.yml --profile migrate run --rm migrate

# Start ONE backend + the frontend
docker compose -f infra/docker-compose.yml --profile rust up -d --build
# or:  --profile c
```

Frontend: `http://<host>:5173`, API: `http://<host>:6767/api`.
On a real host set `CORS_ORIGIN` and `VITE_API_BASE_URL` to the public URLs.

---

## 4. ECR + ECS/Fargate path

1. **Provision RDS for PostgreSQL** (engine 16). Note the endpoint, user, password, DB
   name; build `DATABASE_URL` from them. Put it and `JWT_SECRET` in **AWS Secrets
   Manager** (or SSM Parameter Store) — never in the task definition plaintext.
2. **Run migrations** against RDS before first launch:
   ```sh
   psql "$DATABASE_URL" -f db/migrations/001_init.sql
   psql "$DATABASE_URL" -f db/migrations/002_transaction_type_payment.sql
   psql "$DATABASE_URL" -f db/migrations/003_account_per_type_currency.sql
   ```
   (Or run the compose `migrate` service / `db/migrate.sh` from a bastion with RDS reachable.)
3. **Build & push images** to ECR (repeat per image):
   ```sh
   aws ecr get-login-password --region <region> | docker login --username AWS --password-stdin <acct>.dkr.ecr.<region>.amazonaws.com
   docker build -t gentlix-rust ./backend/rust        # build the rust OR c backend
   docker build -t gentlix-frontend --build-arg VITE_API_BASE_URL=https://api.example.com/api ./frontend/react
   docker tag gentlix-rust <acct>.dkr.ecr.<region>.amazonaws.com/gentlix-rust:latest
   docker push <acct>.dkr.ecr.<region>.amazonaws.com/gentlix-rust:latest
   # repeat for the frontend image
   ```
4. **Task definitions**
   - Backend: container port 6767; inject `DATABASE_URL` + `JWT_SECRET` from Secrets
     Manager; set `LOG_FORMAT=json`, `SERVICE_NAME`, `CORS_ORIGIN`; `awslogs` driver → CloudWatch.
   - Frontend (nginx): container port 80.
5. **Networking / security groups**
   - ALB :443 (TLS via ACM) → frontend target group :80.
   - ALB path/host rule (or a second ALB) → backend target group :6767 for `/api/*`.
   - RDS security group: allow :5432 only from the backend service security group.
6. **CORS**: set backend `CORS_ORIGIN` to the frontend's public origin; rebuild the
   frontend image with `VITE_API_BASE_URL` pointing at the backend's public `/api` URL.

---

## 5. Critical build/runtime caveats (read before deploying)

- **Rust + SQLx offline cache (build blocker).** The Rust DB layer uses
  `sqlx::query!`/`query_as!` macros that are checked against a live database **at
  compile time**. There is currently **no committed `.sqlx/` offline cache**, so the
  Docker build (which has no DB) will fail. Before building the Rust image:
  ```sh
  cd backend/rust
  cargo install sqlx-cli --no-default-features --features postgres
  DATABASE_URL=postgres://user:pass@localhost:5432/gentlix_bank cargo sqlx prepare
  # commit the generated .sqlx/ directory
  ```
  The Dockerfile already sets `SQLX_OFFLINE=true`. (Alternative: build with a reachable
  `DATABASE_URL` instead of the offline cache.)

- **C backend interactive shutdown (Fargate blocker).** `backend/c/main.c` blocks on
  `getchar()` and exits on EOF. In a headless container with no stdin (ECS/Fargate),
  it exits immediately after start. The compose `backend-c` service works around this
  with `stdin_open + tty`; for ECS the cleanest fix is a small code change (wait on
  `SIGTERM`/`SIGINT` instead of `getchar()`) — owned by the backend code, not this doc.

- **`.env` secrets committed to git.** `backend/c/.env` and `backend/rust/.env` are
  tracked in the repository and contain a real DB password and JWT secret. Before any
  deployment: `git rm --cached backend/c/.env backend/rust/.env`, rotate the leaked
  `JWT_SECRET` and Postgres password, and rely on `.env.example` + runtime secrets only.

- **C `setenv` precedence.** On Linux `dotenv.c` overwrites existing env vars, so a
  stray `.env` in the image would override injected secrets. The `.dockerignore`
  excludes `.env`, so container env vars are authoritative — keep it that way.

---

## 6. Deployment readiness checklist

- [ ] Run `cargo sqlx prepare` and commit `backend/rust/.sqlx/` (Rust image build).
- [ ] Decide C backend shutdown strategy for ECS (signal wait) or run it with a TTY.
- [ ] `git rm --cached` the two tracked `.env` files; rotate the exposed secrets.
- [ ] Provision RDS Postgres 16; build `DATABASE_URL`; store it + `JWT_SECRET` in Secrets Manager.
- [ ] Apply migrations `001 → 002 → 003` to RDS (and any seeds you want).
- [ ] `docker build` + `docker push` the chosen backend image and the frontend image to ECR.
- [ ] Build the frontend with the production `VITE_API_BASE_URL`.
- [ ] Create ECS task definitions (ports 6767 / 80), `awslogs` driver, `LOG_FORMAT=json`.
- [ ] ALB + ACM TLS; route `/api/*` to backend:6767, everything else to frontend:80.
- [ ] Security groups: ALB → services; services → RDS:5432 only.
- [ ] Set `CORS_ORIGIN` to the public frontend origin and verify a login round-trip.
- [ ] (Optional) add a backend health endpoint + ALB health check (none exists today;
      target groups currently have to health-check an existing route such as a 404 path).
