BEGIN;

-- Allow one account per (user, type, currency) instead of one per type only.
CREATE UNIQUE INDEX IF NOT EXISTS idx_accounts_user_type_currency
  ON accounts (user_id, account_type, currency);

COMMIT;
