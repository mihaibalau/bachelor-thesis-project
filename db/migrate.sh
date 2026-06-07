#!/usr/bin/env sh
# Run all migrations in order against gentlix_bank.
# Usage: ./db/migrate.sh [psql connection flags]
# Example: ./db/migrate.sh -U mihai -d gentlix_bank

set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PSQL="${PSQL:-psql}"
CONN="${*--d gentlix_bank}"

echo "Applying 001_init.sql..."
$PSQL $CONN -f "$ROOT/db/migrations/001_init.sql"
echo "Applying 002_transaction_type_payment.sql..."
$PSQL $CONN -f "$ROOT/db/migrations/002_transaction_type_payment.sql"
echo "Applying 003_account_per_type_currency.sql..."
$PSQL $CONN -f "$ROOT/db/migrations/003_account_per_type_currency.sql"
echo "Migrations complete."
