# Gentlix k6 benchmarks (Phase 2)

Load tests for the paper table (W1–W4). **Run k6 and the backend on the same machine (Windows)** so `localhost:6767` works.

## Folder layout

```
backend/benchmark/
├── README.md                 # this file
├── run-benchmark.ps1         # Windows: run 3× per workload
├── run-benchmark.sh          # WSL/Linux (optional)
├── summarize-results.ps1     # median p95 + rps from results/*.json
├── k6/
│   ├── lib/config.js         # BASE_URL, warmup/steady, abort if backend down
│   ├── lib/auth.js           # login + load benchmark accounts (W2–W4)
│   ├── w1_login.js
│   ├── w2_statement.js
│   ├── w3_analytics.js
│   └── w4_transfer.js
└── results/                  # k6 JSON summaries (gitignored)
```

## Prerequisites (once)

1. **PostgreSQL** with migrations `001` → `003`.
2. **Benchmark seed** (100k transactions):

   ```powershell
   cd D:\bachelor-thesis-project
   psql -U <user> -d gentlix_bank -f db/seeds/benchmark_seed.sql
   ```

3. **k6 on Windows**:

   ```powershell
   winget install GrafanaLabs.k6
   # new PowerShell window:
   k6 version
   ```

4. Benchmark user: `gentlix@benchmark.com` / `admin`

## Workloads

| ID | Script | VUs | Steady | Endpoint |
|----|--------|-----|--------|----------|
| W1 | `k6/w1_login.js` | 32 | 30s | `POST /api/users/login` |
| W2 | `k6/w2_statement.js` | 8 | 30s | `GET /api/transactions/statement` |
| W3 | `k6/w3_analytics.js` | 8 | 30s | `GET /api/transactions/summary/monthly?per_account_limit=200000` |
| W4 | `k6/w4_transfer.js` | 16 | 30s | `POST /api/transactions/transfer` (1 cent, owned accounts) |

Each run: **15 s warmup** + **30 s steady** (45 s total). Three runs per workload → **median** p95 and rps for the paper.

---

## Run all benchmarks — C backend

**Terminal 1** — start C (port 6767):

```powershell
cd D:\bachelor-thesis-project\backend\c
# build & run c.exe (or your usual start command)
```

**Terminal 2** — verify + run all workloads:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
curl http://localhost:6767/health

# All W1–W4 (3 runs each → results/w*-c-run*.json)
.\run-benchmark.ps1 -Backend c -Workload all
```

Or **one workload at a time**:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
$env:BASE_URL = "http://localhost:6767"

.\run-benchmark.ps1 -Backend c -Workload w1
.\run-benchmark.ps1 -Backend c -Workload w2
.\run-benchmark.ps1 -Backend c -Workload w3

# Re-seed before W4 (transfers change balances):
psql -U <user> -d gentlix_bank -f D:\bachelor-thesis-project\db\seeds\benchmark_seed.sql
.\run-benchmark.ps1 -Backend c -Workload w4
```

Manual k6 (equivalent to one run):

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
$env:BASE_URL = "http://localhost:6767"

k6 run --summary-export results/w1-c-run1.json k6/w1_login.js
k6 run --summary-export results/w1-c-run2.json k6/w1_login.js
k6 run --summary-export results/w1-c-run3.json k6/w1_login.js

k6 run --summary-export results/w2-c-run1.json k6/w2_statement.js
k6 run --summary-export results/w2-c-run2.json k6/w2_statement.js
k6 run --summary-export results/w2-c-run3.json k6/w2_statement.js

k6 run --summary-export results/w3-c-run1.json k6/w3_analytics.js
k6 run --summary-export results/w3-c-run2.json k6/w3_analytics.js
k6 run --summary-export results/w3-c-run3.json k6/w3_analytics.js

psql -U <user> -d gentlix_bank -f D:\bachelor-thesis-project\db\seeds\benchmark_seed.sql
k6 run --summary-export results/w4-c-run1.json k6/w4_transfer.js
k6 run --summary-export results/w4-c-run2.json k6/w4_transfer.js
k6 run --summary-export results/w4-c-run3.json k6/w4_transfer.js
```

---

## Run all benchmarks — Rust backend

**Stop C.** Start Rust on the **same port 6767**.

**Terminal 1** — Rust:

```powershell
cd D:\bachelor-thesis-project\backend\rust
cargo run
```

**Terminal 2** — same commands, replace `c` with `rust`:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
curl http://localhost:6767/health

.\run-benchmark.ps1 -Backend rust -Workload all
```

Or per workload:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
$env:BASE_URL = "http://localhost:6767"

.\run-benchmark.ps1 -Backend rust -Workload w1
.\run-benchmark.ps1 -Backend rust -Workload w2
.\run-benchmark.ps1 -Backend rust -Workload w3

psql -U <user> -d gentlix_bank -f D:\bachelor-thesis-project\db\seeds\benchmark_seed.sql
.\run-benchmark.ps1 -Backend rust -Workload w4
```

Manual k6:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
$env:BASE_URL = "http://localhost:6767"

k6 run --summary-export results/w1-rust-run1.json k6/w1_login.js
k6 run --summary-export results/w1-rust-run2.json k6/w1_login.js
k6 run --summary-export results/w1-rust-run3.json k6/w1_login.js

k6 run --summary-export results/w2-rust-run1.json k6/w2_statement.js
k6 run --summary-export results/w2-rust-run2.json k6/w2_statement.js
k6 run --summary-export results/w2-rust-run3.json k6/w2_statement.js

k6 run --summary-export results/w3-rust-run1.json k6/w3_analytics.js
k6 run --summary-export results/w3-rust-run2.json k6/w3_analytics.js
k6 run --summary-export results/w3-rust-run3.json k6/w3_analytics.js

psql -U <user> -d gentlix_bank -f D:\bachelor-thesis-project\db\seeds\benchmark_seed.sql
k6 run --summary-export results/w4-rust-run1.json k6/w4_transfer.js
k6 run --summary-export results/w4-rust-run2.json k6/w4_transfer.js
k6 run --summary-export results/w4-rust-run3.json k6/w4_transfer.js
```

---

## Read results (paper table)

After C and Rust runs:

```powershell
cd D:\bachelor-thesis-project\backend\benchmark
.\summarize-results.ps1
```

Or from each `results/w*-{c|rust}-run*.json`:

| Paper column | JSON path |
|--------------|-----------|
| **p95 (ms)** | `metrics.http_req_duration["p(95)"]` |
| **rps** | `metrics.http_reqs.rate` |

Use the **median of three runs** per backend per workload.

Example paper row (W1, conc 32):

| Wkld | Conc. | C p95 | Rust p95 | C rps | Rust rps |
|------|-------|-------|----------|-------|----------|
| W1 Login | 32 | *(median c)* | *(median rust)* | *(median c)* | *(median rust)* |

---

## Environment variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `BASE_URL` | `http://localhost:6767` | Backend root (no `/api` suffix) |
| `BENCH_EMAIL` | `gentlix@benchmark.com` | Login user |
| `BENCH_PASSWORD` | `admin` | Login password |
| `STATEMENT_FROM` | `2024-01-01` | W2 date filter |
| `STATEMENT_TO` | `2026-12-31` | W2 date filter |
| `PER_ACCOUNT_LIMIT` | `200000` | W3 rows per account |

---

## WSL (optional)

Only use WSL if both k6 and backend run there. If the backend is on **Windows**, prefer **PowerShell + k6 on Windows** (sections above).

```bash
cd /mnt/d/bachelor-thesis-project/backend/benchmark
chmod +x run-benchmark.sh
sed -i 's/\r$//' run-benchmark.sh   # if bash\r error

./run-benchmark.sh -b c -w all
# stop C, start Rust:
./run-benchmark.sh -b rust -w all
```

If k6 is in WSL but backend on Windows:

```bash
WIN_HOST=$(grep -m1 nameserver /etc/resolv.conf | awk '{print $2}')
./run-benchmark.sh -b c -w w1 -u "http://${WIN_HOST}:6767"
```
