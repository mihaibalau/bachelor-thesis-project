-- Seed dashboard demo data for mihaiblu@gmail.com
-- Prerequisites: run migrations 001 → 002 → 003, then register mihaiblu@gmail.com with a Regular RON account.
-- Usage: psql -U USER -d gentlix_bank -f db/seeds/dashboard_demo.sql

BEGIN;

DO $$
DECLARE
    v_user_id BIGINT;
    v_account_id BIGINT;
    v_bank_account_id BIGINT;
    v_andrei_acct BIGINT;
    v_maria_acct BIGINT;
    v_ion_acct BIGINT;
    v_andrei_user BIGINT;
    v_maria_user BIGINT;
    v_ion_user BIGINT;
BEGIN
    SELECT id INTO v_user_id FROM users WHERE email = 'mihaiblu@gmail.com';
    IF v_user_id IS NULL THEN
        RAISE EXCEPTION 'User mihaiblu@gmail.com not found. Register first, then re-run this seed.';
    END IF;

    SELECT a.id INTO v_bank_account_id
    FROM accounts a
    JOIN users u ON u.id = a.user_id
    WHERE u.tag = '__bank__'
    LIMIT 1;
    IF v_bank_account_id IS NULL THEN
        RAISE EXCEPTION 'Bank vault account not found. Run migration 001 first.';
    END IF;

    SELECT id INTO v_account_id
    FROM accounts
    WHERE user_id = v_user_id AND account_type = 'Regular' AND currency = 'RON'
    LIMIT 1;

    IF v_account_id IS NULL THEN
        RAISE EXCEPTION 'Regular RON account not found for mihaiblu@gmail.com';
    END IF;

    -- Recipient users for affiliates (create if missing)
    INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
    VALUES ('andrei_demo', 'andrei.demo@gentlix.test', 'Andrei', 'Popescu', NULL, NULL,
            '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I')
    ON CONFLICT (tag) DO NOTHING;

    INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
    VALUES ('maria_demo', 'maria.demo@gentlix.test', 'Maria', 'Ionescu', NULL, NULL,
            '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I')
    ON CONFLICT (tag) DO NOTHING;

    INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
    VALUES ('ion_demo', 'ion.demo@gentlix.test', 'Ion', 'Vasilescu', NULL, NULL,
            '$argon2id$v=19$m=65536,t=3,p=1$o0+BWRVwggGxBCH91PdySA$g9QoISq+S3jS1bO6MAyJZGlL5ljn2VXuHWYdal9CO7I')
    ON CONFLICT (tag) DO NOTHING;

    SELECT id INTO v_andrei_user FROM users WHERE tag = 'andrei_demo';
    SELECT id INTO v_maria_user FROM users WHERE tag = 'maria_demo';
    SELECT id INTO v_ion_user FROM users WHERE tag = 'ion_demo';

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_andrei_user, 'Regular', 'RON', 0, 'RO49GTLX0000000000000001'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000001');

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_maria_user, 'Regular', 'RON', 0, 'RO49GTLX0000000000000002'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000002');

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_ion_user, 'Regular', 'RON', 0, 'RO49GTLX0000000000000003'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000003');

    SELECT id INTO v_andrei_acct FROM accounts WHERE iban = 'RO49GTLX0000000000000001';
    SELECT id INTO v_maria_acct FROM accounts WHERE iban = 'RO49GTLX0000000000000002';
    SELECT id INTO v_ion_acct FROM accounts WHERE iban = 'RO49GTLX0000000000000003';

    -- Reset balances for demo accounts before replaying ledger
    UPDATE accounts SET balance_cents = 0
    WHERE id IN (v_account_id, v_andrei_acct, v_maria_acct);

    -- Clear previous demo transactions for this account
    DELETE FROM transactions
    WHERE from_account_id = v_account_id OR to_account_id = v_account_id;

    -- Affiliates
    DELETE FROM affiliates WHERE owner_user_id = v_user_id;
    INSERT INTO affiliates (owner_user_id, recipient_sub_account_id, nickname) VALUES
        (v_user_id, v_andrei_acct, 'Andrei'),
        (v_user_id, v_maria_acct, 'Maria'),
        (v_user_id, v_ion_acct, 'Ion');

    -- Previous month spending (May 2026) — higher than June payments for ~-8% trend
    INSERT INTO transactions (from_account_id, to_account_id, transaction_type, value_cents, recorded_on, description) VALUES
        (v_account_id, v_bank_account_id, 'Payment', 20000, '2026-05-05 10:00:00+00', 'Payment | category: Shopping | merchant: eMAG'),
        (v_account_id, v_bank_account_id, 'Payment', 15000, '2026-05-12 14:30:00+00', 'Payment | category: Food | merchant: Restaurant'),
        (v_account_id, v_bank_account_id, 'Payment', 12000, '2026-05-18 09:15:00+00', 'Payment | category: Shopping | merchant: H&M'),
        (v_account_id, v_bank_account_id, 'Withdrawal', 15000, '2026-05-20 16:00:00+00', 'ATM Withdrawal'),
        (v_account_id, v_bank_account_id, 'Payment', 8324,  '2026-05-22 11:00:00+00', 'Payment | category: Food | merchant: FoodKit'),
        (v_account_id, v_andrei_acct, 'Send', 5000, '2026-05-25 18:00:00+00', 'Send: Rent share');

    -- June 2026 current month activity (matches dashboard mock)
    INSERT INTO transactions (from_account_id, to_account_id, transaction_type, value_cents, recorded_on, description) VALUES
        (v_account_id, v_bank_account_id, 'Payment', 12675, '2026-06-02 09:36:00+00', 'Payment | category: Groceries & household | merchant: Mega Image'),
        (v_account_id, v_bank_account_id, 'Payment', 8950,  '2026-06-02 20:18:00+00', 'Payment | category: Food | merchant: Glovo | note: Dinner with friends'),
        (v_account_id, v_bank_account_id, 'Withdrawal', 40000, '2026-06-03 13:02:00+00', 'ATM Withdrawal'),
        (v_account_id, v_bank_account_id, 'Payment', 2999,  '2026-06-04 07:11:00+00', 'Payment | category: Subscription | merchant: Spotify'),
        (v_account_id, v_andrei_acct, 'Send', 95000, '2026-06-05 18:42:00+00', 'Send: Andrei | note: Rent share'),
        (v_bank_account_id, v_account_id, 'Deposit', 650000, '2026-06-06 08:00:00+00', 'Salary | Gentlix Bank'),
        (v_account_id, v_maria_acct, 'Send', 325000, '2026-06-06 11:00:00+00', 'Send: Maria | note: Project payment');

    -- Reconcile balances from ledger (matches service-layer credit/debit semantics)
    UPDATE accounts a
    SET balance_cents = COALESCE(ledger.computed, 0)
    FROM (
        SELECT acc.id,
            SUM(
                CASE
                    WHEN t.to_account_id = acc.id THEN t.value_cents
                    WHEN t.from_account_id = acc.id THEN -t.value_cents
                    ELSE 0
                END
            ) AS computed
        FROM accounts acc
        LEFT JOIN transactions t
            ON t.from_account_id = acc.id OR t.to_account_id = acc.id
        WHERE acc.id IN (v_account_id, v_andrei_acct, v_maria_acct)
        GROUP BY acc.id
    ) ledger
    WHERE a.id = ledger.id;

    RAISE NOTICE 'Dashboard seed complete for user % (account %)', v_user_id, v_account_id;
END $$;

COMMIT;
