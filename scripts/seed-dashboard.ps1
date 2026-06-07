# Seeds dashboard demo data for mihaiblu@gmai.com via PostgreSQL.
# Requires psql in PATH and a running gentlix_bank database.

$ErrorActionPreference = "Stop"

$DbUser = if ($env:POSTGRES_USER) { $env:POSTGRES_USER } else { "mihai" }
$DbName = if ($env:POSTGRES_DB) { $env:POSTGRES_DB } else { "gentlix_bank" }
$DbHost = if ($env:POSTGRES_HOST) { $env:POSTGRES_HOST } else { "localhost" }
$DbPort = if ($env:POSTGRES_PORT) { $env:POSTGRES_PORT } else { "5432" }

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Migration = Join-Path $ProjectRoot "db\migrations\002_transaction_type_payment.sql"
$Seed = Join-Path $ProjectRoot "db\seeds\dashboard_demo.sql"

Write-Host "Applying migration 002 (Payment transaction type)..."
& psql -h $DbHost -p $DbPort -U $DbUser -d $DbName -f $Migration

Write-Host "Seeding dashboard demo data for mihaiblu@gmai.com..."
& psql -h $DbHost -p $DbPort -U $DbUser -d $DbName -f $Seed

Write-Host "Done. Log in with mihaiblu@gmai.com / 08102004 and open the dashboard."
