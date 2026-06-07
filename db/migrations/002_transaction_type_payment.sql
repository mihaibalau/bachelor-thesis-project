BEGIN;

ALTER TABLE transactions DROP CONSTRAINT IF EXISTS transactions_type_check;

ALTER TABLE transactions ADD CONSTRAINT transactions_type_check
  CHECK (transaction_type IN ('Deposit', 'Withdrawal', 'Send', 'Transfer', 'Payment'));

COMMIT;
