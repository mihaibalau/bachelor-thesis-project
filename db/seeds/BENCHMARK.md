# Benchmark seed

Stress-test data for `gentlix@benchmark.com` / `admin`.

## One seed file

| File | Rows | Notes |
|------|------|-------|
| **`benchmark_seed.sql`** | **100,000 exactly** | All types; RON + Savings + EUR; dates 2024-01-01 to today |

Re-running **replaces** all benchmark transactions.

## Why Statistics showed "500 transactions"

The summary API loads at most `per_account_limit` rows **per account** (default **500**). The seed had 100k rows in the DB, but the UI only aggregated 500.

Use `per_account_limit=200000` when benchmarking:

```
GET /api/transactions/summary?from=2024-01-01&to=2026-06-07&per_account_limit=200000
```

The Statistics page now passes `per_account_limit=200000` automatically.

## PowerShell

```powershell
$env:PGPASSWORD = "<your-postgres-password>"
psql -U <your-postgres-user> -d gentlix_bank -f .\db\seeds\benchmark_seed.sql
```

Or use the helper script (defaults to `-U mihai -d gentlix_bank`, override by passing psql flags):

```sh
sh db/seeds/run_benchmark.sh -U <your-postgres-user> -d gentlix_bank
```

Expect ~15-45 seconds for 100k inserts.

## Timing C vs Rust

1. Run the seed once.
2. Start one backend (C or Rust) on port 6767.
3. Log in as `gentlix@benchmark.com` / `admin`.
4. Open Statistics with date range **2024-01-01** to **today**, scope **All accounts**.
5. Or call the summary URL above and measure response time.

Compare the same request on C and Rust backends.
