-- Benchmark seed: exactly 100,000 transaction ROWS for stress testing.
--
-- Login:  gentlix@benchmark.com  /  admin
--
-- Covers all types (Deposit, Withdrawal, Payment, Transfer, Send) across
-- Regular RON, Savings RON, and Regular EUR. Dates from 2024-01-01 through
-- CURRENT_DATE. Re-running replaces all benchmark ledger data.
--
-- Statistics API must use per_account_limit >= 100000 (default was 500).
--
-- Usage:
--   psql -U mihai -d gentlix_bank -f db/seeds/benchmark_seed.sql

BEGIN;

DO $$
DECLARE
    v_tx_count        INT  := 100000;

    v_start_date      DATE := DATE '2024-01-01';
    v_day_span        INT;

    v_user_id         BIGINT;
    v_bank_id         BIGINT;
    v_ron_id          BIGINT;
    v_savings_id      BIGINT;
    v_eur_id          BIGINT;
    v_peer1_user      BIGINT;
    v_peer2_user      BIGINT;
    v_peer1_acct      BIGINT;
    v_peer2_acct      BIGINT;

    v_password_hash   TEXT := '$argon2id$v=19$m=19456,t=2,p=1$XASY3AW5lvFkAXwNHIf3rA$xwUBroXXOdGjaI6Z5dDtng+PDZc9+OIM0pv/+nCaGyU';

    v_inserted        BIGINT;
    v_user_tx_count   BIGINT;
BEGIN
    v_day_span := CURRENT_DATE - v_start_date;
    IF v_day_span < 1 THEN
        v_day_span := 1;
    END IF;

    RAISE NOTICE 'Benchmark seed: % transaction rows, dates % to %',
        v_tx_count, v_start_date, CURRENT_DATE;

    SELECT a.id INTO v_bank_id
    FROM accounts a
    JOIN users u ON u.id = a.user_id
    WHERE u.tag = '__bank__'
    LIMIT 1;
    IF v_bank_id IS NULL THEN
        RAISE EXCEPTION 'Bank vault account missing - run migration 001 first.';
    END IF;

    INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
    VALUES (
        'gentlix_benchmark',
        'gentlix@benchmark.com',
        'Benchmark',
        'Gentlix',
        NULL,
        NULL,
        v_password_hash
    )
    ON CONFLICT (tag) DO UPDATE SET
        email = EXCLUDED.email,
        password_hash = EXCLUDED.password_hash;

    SELECT id INTO v_user_id FROM users WHERE tag = 'gentlix_benchmark';

    INSERT INTO users (tag, email, first_name, last_name, phone, birth_date, password_hash)
    VALUES
        ('bench_peer_1', 'bench.peer1@gentlix.test', 'Peer', 'One', NULL, NULL, v_password_hash),
        ('bench_peer_2', 'bench.peer2@gentlix.test', 'Peer', 'Two', NULL, NULL, v_password_hash)
    ON CONFLICT (tag) DO NOTHING;

    SELECT id INTO v_peer1_user FROM users WHERE tag = 'bench_peer_1';
    SELECT id INTO v_peer2_user FROM users WHERE tag = 'bench_peer_2';

    -- IBANs must be exactly 24 chars (Rust IBAN validation); old 25-char BENCH* values are migrated below.
    UPDATE accounts SET iban = 'RO49GTLX0000000000000B01' WHERE iban = 'RO49GTLX0000000000BENCH01';
    UPDATE accounts SET iban = 'RO49GTLX0000000000000B02' WHERE iban = 'RO49GTLX0000000000BENCH02';
    UPDATE accounts SET iban = 'RO49GTLX0000000000000B03' WHERE iban = 'RO49GTLX0000000000BENCH03';
    UPDATE accounts SET iban = 'RO49GTLX000000000000BP01' WHERE iban = 'RO49GTLX0000000000BENCHP1';
    UPDATE accounts SET iban = 'RO49GTLX000000000000BP02' WHERE iban = 'RO49GTLX0000000000BENCHP2';

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_peer1_user, 'Regular', 'RON', 0, 'RO49GTLX000000000000BP01'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX000000000000BP01');

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_peer2_user, 'Regular', 'RON', 0, 'RO49GTLX000000000000BP02'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX000000000000BP02');

    SELECT id INTO v_peer1_acct FROM accounts WHERE iban = 'RO49GTLX000000000000BP01';
    SELECT id INTO v_peer2_acct FROM accounts WHERE iban = 'RO49GTLX000000000000BP02';

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_user_id, 'Regular', 'RON', 0, 'RO49GTLX0000000000000B01'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000B01');

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_user_id, 'Savings', 'RON', 0, 'RO49GTLX0000000000000B02'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000B02');

    INSERT INTO accounts (user_id, account_type, currency, balance_cents, iban)
    SELECT v_user_id, 'Regular', 'EUR', 0, 'RO49GTLX0000000000000B03'
    WHERE NOT EXISTS (SELECT 1 FROM accounts WHERE iban = 'RO49GTLX0000000000000B03');

    SELECT id INTO v_ron_id     FROM accounts WHERE iban = 'RO49GTLX0000000000000B01';
    SELECT id INTO v_savings_id FROM accounts WHERE iban = 'RO49GTLX0000000000000B02';
    SELECT id INTO v_eur_id     FROM accounts WHERE iban = 'RO49GTLX0000000000000B03';

    DELETE FROM affiliates WHERE owner_user_id = v_user_id;
    INSERT INTO affiliates (owner_user_id, recipient_sub_account_id, nickname) VALUES
        (v_user_id, v_peer1_acct, 'Bench Peer 1'),
        (v_user_id, v_peer2_acct, 'Bench Peer 2');

    DELETE FROM transactions
    WHERE from_account_id IN (v_ron_id, v_savings_id, v_eur_id, v_peer1_acct, v_peer2_acct)
       OR to_account_id   IN (v_ron_id, v_savings_id, v_eur_id, v_peer1_acct, v_peer2_acct);

    UPDATE accounts SET balance_cents = 0
    WHERE id IN (v_ron_id, v_savings_id, v_eur_id, v_peer1_acct, v_peer2_acct);

    -- Exactly 100,000 rows. 12-step cycle ((n-1) % 12):
    --   0 Deposit RON, 1 Payment RON, 2 Withdrawal RON, 3 Deposit RON (bank legs net 0)
    --   4 Transfer RON->Savings, 5 Transfer Savings->RON (internal net 0)
    --   6 Send peer1, 7 Send peer2
    --   8 Deposit EUR, 9 Payment EUR, 10 Withdrawal EUR, 11 Deposit EUR (EUR bank net 0)
    INSERT INTO transactions (from_account_id, to_account_id, transaction_type, value_cents, recorded_on, description)
    SELECT
        CASE ((n - 1) % 12)
            WHEN 0 THEN v_bank_id      WHEN 1 THEN v_ron_id
            WHEN 2 THEN v_ron_id       WHEN 3 THEN v_bank_id
            WHEN 4 THEN v_ron_id       WHEN 5 THEN v_savings_id
            WHEN 6 THEN v_ron_id       WHEN 7 THEN v_ron_id
            WHEN 8 THEN v_bank_id      WHEN 9 THEN v_eur_id
            WHEN 10 THEN v_eur_id      WHEN 11 THEN v_bank_id
        END,
        CASE ((n - 1) % 12)
            WHEN 0 THEN v_ron_id       WHEN 1 THEN v_bank_id
            WHEN 2 THEN v_bank_id      WHEN 3 THEN v_ron_id
            WHEN 4 THEN v_savings_id   WHEN 5 THEN v_ron_id
            WHEN 6 THEN v_peer1_acct   WHEN 7 THEN v_peer2_acct
            WHEN 8 THEN v_eur_id       WHEN 9 THEN v_bank_id
            WHEN 10 THEN v_bank_id     WHEN 11 THEN v_eur_id
        END,
        CASE ((n - 1) % 12)
            WHEN 0 THEN 'Deposit'      WHEN 1 THEN 'Payment'
            WHEN 2 THEN 'Withdrawal'   WHEN 3 THEN 'Deposit'
            WHEN 4 THEN 'Transfer'     WHEN 5 THEN 'Transfer'
            WHEN 6 THEN 'Send'         WHEN 7 THEN 'Send'
            WHEN 8 THEN 'Deposit'      WHEN 9 THEN 'Payment'
            WHEN 10 THEN 'Withdrawal'  WHEN 11 THEN 'Deposit'
        END,
        CASE
            WHEN n = 1 THEN 1000000                         -- 10,000 RON opening
            WHEN ((n - 1) % 12) IN (6, 7) THEN 10 + (n % 5) * 10   -- tiny sends; keep RON balance positive
            ELSE 2000 + (((n - 1) / 12) % 30) * 100                 -- same amount per 12-step cycle
        END,
        (v_start_date::timestamp AT TIME ZONE 'UTC')
            + ((n % (v_day_span + 1)) * INTERVAL '1 day')
            + ((n % 1440) * INTERVAL '1 minute'),
        CASE ((n - 1) % 12)
            WHEN 0 THEN 'Benchmark opening deposit #' || n
            WHEN 1 THEN 'Payment | category: '
                || (ARRAY['Food','Shopping','Transport','Bills','Entertainment','Other'])[1 + (n % 6)]
                || ' | merchant: Bench-' || n
            WHEN 2 THEN 'ATM Withdrawal (benchmark #' || n || ')'
            WHEN 3 THEN 'Benchmark rebalance deposit #' || n
            WHEN 4 THEN 'Transfer to savings #' || n
            WHEN 5 THEN 'Transfer from savings #' || n
            WHEN 6 THEN 'Send: Bench Peer 1 | note: #' || n
            WHEN 7 THEN 'Send: Bench Peer 2 | note: #' || n
            WHEN 8 THEN 'EUR benchmark deposit #' || n
            WHEN 9 THEN 'Payment | category: Shopping | merchant: EUR-Bench-' || n
            WHEN 10 THEN 'ATM Withdrawal EUR (benchmark #' || n || ')'
            WHEN 11 THEN 'EUR rebalance deposit #' || n
        END
    FROM generate_series(1, v_tx_count) AS n;

    GET DIAGNOSTICS v_inserted = ROW_COUNT;

    UPDATE accounts a
    SET balance_cents = GREATEST(COALESCE(ledger.computed, 0), 0)
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
        WHERE acc.id IN (v_ron_id, v_savings_id, v_eur_id, v_peer1_acct, v_peer2_acct)
        GROUP BY acc.id
    ) ledger
    WHERE a.id = ledger.id;

    SELECT COUNT(DISTINCT t.id) INTO v_user_tx_count
    FROM transactions t
    WHERE t.from_account_id IN (v_ron_id, v_savings_id, v_eur_id)
       OR t.to_account_id   IN (v_ron_id, v_savings_id, v_eur_id);

    ANALYZE transactions;
    ANALYZE accounts;

    RAISE NOTICE 'Inserted rows: %', v_inserted;
    RAISE NOTICE 'Distinct transactions touching benchmark user accounts: %', v_user_tx_count;
    RAISE NOTICE 'Regular RON balance: % cents', (SELECT balance_cents FROM accounts WHERE id = v_ron_id);
    RAISE NOTICE 'Savings RON balance: % cents', (SELECT balance_cents FROM accounts WHERE id = v_savings_id);
    RAISE NOTICE 'Regular EUR balance: % cents', (SELECT balance_cents FROM accounts WHERE id = v_eur_id);
    RAISE NOTICE 'Login: gentlix@benchmark.com / admin';
    RAISE NOTICE 'API: GET /api/transactions/summary?from=2024-01-01&to=2026-06-07&per_account_limit=200000';
END $$;

COMMIT;
