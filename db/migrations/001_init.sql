
BEGIN;

CREATE TABLE IF NOT EXISTS users (
  id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  tag           TEXT NOT NULL UNIQUE,
  email         TEXT NOT NULL UNIQUE,
  first_name    TEXT NOT NULL,
  last_name     TEXT NOT NULL,
  phone         TEXT,
  birth_date    DATE,
  password_hash TEXT NOT NULL
);


CREATE TABLE IF NOT EXISTS accounts (
  id            BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  user_id       BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,

  account_type  TEXT NOT NULL,
  currency      CHAR(3) NOT NULL,

  balance_cents BIGINT NOT NULL DEFAULT 0,
  iban          TEXT NOT NULL UNIQUE,

  CONSTRAINT accounts_balance_cents_non_negative CHECK (balance_cents >= 0),
  CONSTRAINT accounts_account_type_check CHECK (account_type IN ('Savings', 'Credit', 'Regular')),
  CONSTRAINT accounts_currency_check CHECK (currency IN ('RON', 'EUR', 'USD'))
);


CREATE TABLE IF NOT EXISTS affiliates (
  owner_user_id            BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  recipient_sub_account_id BIGINT NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  nickname                 TEXT NOT NULL,

  PRIMARY KEY (owner_user_id, recipient_sub_account_id),

  CONSTRAINT affiliates_nickname_non_empty CHECK (length(trim(nickname)) > 0)
);


CREATE TABLE IF NOT EXISTS transactions (
  id               BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  from_account_id  BIGINT NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
  to_account_id    BIGINT NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,

  transaction_type TEXT NOT NULL,
  value_cents      BIGINT NOT NULL,

  recorded_on      TIMESTAMPTZ NOT NULL DEFAULT now(),
  description      TEXT NOT NULL,

  CONSTRAINT transactions_value_cents_non_negative CHECK (value_cents >= 0),
  CONSTRAINT transactions_type_check CHECK (transaction_type IN ('deposit', 'withdrawal', 'send', 'transfer')),
  CONSTRAINT transactions_from_to_different CHECK (from_account_id <> to_account_id),
  CONSTRAINT transactions_description_non_empty CHECK (length(trim(description)) > 0)
);


CREATE INDEX IF NOT EXISTS idx_accounts_user_id ON accounts(user_id);

CREATE INDEX IF NOT EXISTS idx_affiliates_owner_user_id ON affiliates(owner_user_id);
CREATE INDEX IF NOT EXISTS idx_affiliates_recipient_sub_account_id ON affiliates(recipient_sub_account_id);

CREATE INDEX IF NOT EXISTS idx_transactions_from_account_id ON transactions(from_account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_to_account_id ON transactions(to_account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_recorded_on ON transactions(recorded_on);


CREATE TABLE IF NOT EXISTS admins (
  id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,

  username TEXT NOT NULL UNIQUE,
  password_hash TEXT NOT NULL,

  role TEXT NOT NULL,
  created_by_admin_id BIGINT REFERENCES admins(id) ON DELETE RESTRICT,

  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),

  CONSTRAINT admins_role_check CHECK (role IN ('ROOT', 'OPERATOR', 'SUPERVISOR')),
  CONSTRAINT admins_root_has_no_creator CHECK (
    (role = 'ROOT' AND created_by_admin_id IS NULL)
    OR
    (role <> 'ROOT' AND created_by_admin_id IS NOT NULL)
  )
);

CREATE TABLE IF NOT EXISTS admin_audit_log (
  id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  admin_id BIGINT NOT NULL REFERENCES admins(id) ON DELETE RESTRICT,
  action TEXT NOT NULL,
  target_type TEXT,
  target_key TEXT,
  metadata JSONB,
  recorded_on TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_admin_audit_log_admin_id ON admin_audit_log(admin_id);
CREATE INDEX IF NOT EXISTS idx_admin_audit_log_recorded_on ON admin_audit_log(recorded_on);

-- =========================================================
-- INIT ENTITIES
-- =========================================================
INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
VALUES (
  '__bank__',
  'contact@bank-gentlix.ro',
  'BANK',
  'SYSTEM',
  NULL,
  NULL,
  '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I'
)
ON CONFLICT (tag) DO NOTHING;

INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
SELECT u.id, 'Regular', 'RON', 1000000, 'RO00GTLXVAULT000000000000'  -- 10,000.00 RON in cents
FROM users u
WHERE u.tag = '__bank__'
ON CONFLICT (iban) DO NOTHING;

INSERT INTO admins (username, password_hash, role, created_by_admin_id)
VALUES (
  'bank_admin',
  '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I',
  'ROOT',
  NULL
)
ON CONFLICT (username) DO NOTHING;

COMMIT;
