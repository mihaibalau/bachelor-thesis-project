#!/usr/bin/env sh
# Run benchmark seed. Usage: ./db/seeds/run_benchmark.sh [psql args]
# Example: ./db/seeds/run_benchmark.sh -U mihai -d gentlix_bank

set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PSQL="${PSQL:-psql}"
CONN="${*--U mihai -d gentlix_bank}"

echo "Applying benchmark seed (this may take 1-3 minutes for 100k rows)..."
$PSQL $CONN -f "$ROOT/db/seeds/benchmark_seed.sql"
echo "Done. Login: gentlix@benchmark.com / admin"
