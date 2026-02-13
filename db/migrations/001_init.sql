-- db/migrations/001_init.sql
BEGIN;

-- USERS
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

-- SUB ACCOUNTS (MAIN/SAVINGS/CREDIT_CARD etc.)
CREATE TABLE IF NOT EXISTS sub_accounts (
  id        BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  user_id   BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  type      TEXT NOT NULL,
  currency  CHAR(3) NOT NULL DEFAULT 'RON',
  balance   NUMERIC(18,2) NOT NULL DEFAULT 0,
  iban 		TEXT NOT NULL UNIQUE,

  CONSTRAINT sub_accounts_balance_non_negative CHECK (balance >= 0),
  CONSTRAINT sub_accounts_type_check CHECK (type IN ('MAIN', 'SAVINGS', 'CREDIT_CARD'))
);

-- AFFILIATES: shortcut
CREATE TABLE IF NOT EXISTS affiliates (
  owner_user_id            BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  recipient_sub_account_id BIGINT NOT NULL REFERENCES sub_accounts(id) ON DELETE CASCADE,
  nickname                 TEXT,
  
  PRIMARY KEY (owner_user_id, recipient_sub_account_id)
);


-- TRANSACTIONS
CREATE TABLE IF NOT EXISTS transactions (
  id                 BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  from_sub_account_id BIGINT NOT NULL REFERENCES sub_accounts(id) ON DELETE RESTRICT,
  to_sub_account_id   BIGINT NOT NULL REFERENCES sub_accounts(id) ON DELETE RESTRICT,
  type               TEXT NOT NULL,
  amount             NUMERIC(18,2) NOT NULL,
  recorded_on        TIMESTAMPTZ NOT NULL DEFAULT now(),
  description        TEXT,

  CONSTRAINT transactions_amount_positive CHECK (amount > 0),
  CONSTRAINT transactions_type_check CHECK (type IN ('DEPOSIT', 'WITHDRAW', 'SEND', 'TRANSFER')),
  CONSTRAINT transactions_from_to_different CHECK (from_sub_account_id <> to_sub_account_id)

);

CREATE INDEX IF NOT EXISTS idx_sub_accounts_user_id ON sub_accounts(user_id);
CREATE INDEX IF NOT EXISTS idx_affiliates_owner_user_id ON affiliates(owner_user_id);
CREATE INDEX IF NOT EXISTS idx_transactions_from_sub_account_id ON transactions(from_sub_account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_to_sub_account_id ON transactions(to_sub_account_id);


-- ADMIN MODE

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
  action TEXT NOT NULL,                 -- ex: CREATE_ADMIN, DELETE_ADMIN, MANUAL_DEPOSIT
  target_type TEXT,                     -- ex: admin/user/sub_account/transaction
  target_key TEXT,                      
  metadata JSONB,                       -- details; C - libpq, Rust - sqlx
  recorded_on TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_admin_audit_log_admin_id ON admin_audit_log(admin_id);
CREATE INDEX IF NOT EXISTS idx_admin_audit_log_recorded_on ON admin_audit_log(recorded_on);


-- INIT ENTITIES

INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
VALUES ('__bank__', 'contact@bank-gentlix.ro', 'BANK', 'SYSTEM', NULL, NULL, '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I')
ON CONFLICT (tag) DO NOTHING;  -- safe re-run


INSERT INTO sub_accounts (user_id, type, currency, balance, iban)
SELECT u.id, 'MAIN', 'RON', 10000, 'RO00GTLXVAULT000000000000'
FROM users u
WHERE u.tag = '__bank__'
ON CONFLICT (iban) DO NOTHING;  -- safe re-run


INSERT INTO admins (username, password_hash, role, created_by_admin_id)
VALUES ('bank_admin', '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I', 'ROOT', NULL)
ON CONFLICT (username) DO NOTHING;  -- safe re-run 

COMMIT;
