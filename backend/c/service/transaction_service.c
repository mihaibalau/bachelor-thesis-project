#include "include/transaction_service.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Cross-platform processor count — replaces POSIX-only sysconf */
#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#endif

/*
 * The "bank" account that funds ATM deposits and receives withdrawals /
 * payments. Mirrors Rust's `const BANK_ACCOUNT_ID: AccountId = AccountId(1)`.
 */
static const AccountId BANK_ACCOUNT_ID = { 1 };

/*  Adapters: DB repos -> abstract repo interfaces  (§3.3)          */

static bool tx_repo_get_by_id_adapter(
    void *ctx, TransactionId id, Transaction **out, RepoError *err)
{
    return transaction_repo_get_by_id((TransactionRepo *)ctx, id, out, err);
}

static bool tx_repo_insert_adapter(
    void *ctx, const Transaction *tx, TransactionId *out_id, RepoError *err)
{
    return transaction_repo_insert((TransactionRepo *)ctx, tx, out_id, err);
}

static bool tx_repo_list_for_account_adapter(
    void *ctx, AccountId account_id, int64_t limit, int64_t offset,
    Transaction ***out_txs, size_t *out_count, RepoError *err)
{
    return transaction_repo_list_for_account(
        (TransactionRepo *)ctx, account_id, limit, offset, out_txs, out_count, err);
}

static const TransactionRepositoryVTable TX_REPO_VTABLE = {
    tx_repo_get_by_id_adapter,
    tx_repo_insert_adapter,
    tx_repo_list_for_account_adapter
};

TransactionRepository tx_repository_from_repo(TransactionRepo *repo) {
    TransactionRepository r;
    r.vtable = &TX_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

static bool tx_acct_repo_list_for_user_adapter(
    void *ctx, UserId user_id,
    Account ***out_accounts, size_t *out_count, RepoError *err)
{
    return account_repo_list_for_user(
        (AccountRepo *)ctx, user_id, out_accounts, out_count, err);
}

static bool tx_acct_repo_get_by_id_adapter(
    void *ctx, AccountId account_id, Account **out, RepoError *err)
{
    return account_repo_get_by_id((AccountRepo *)ctx, account_id, out, err);
}

static bool tx_acct_repo_update_adapter(
    void *ctx, const Account *account, RepoError *err)
{
    return account_repo_update((AccountRepo *)ctx, account, err);
}

static const TxAccountRepositoryVTable TX_ACCT_REPO_VTABLE = {
    tx_acct_repo_list_for_user_adapter,
    tx_acct_repo_get_by_id_adapter,
    tx_acct_repo_update_adapter
};

TxAccountRepository tx_account_repository_from_repo(AccountRepo *repo) {
    TxAccountRepository r;
    r.vtable = &TX_ACCT_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

/*  TransactionService struct   (§3.1)                               */

struct TransactionService {
    TransactionRepository tx_repo;
    TxAccountRepository   account_repo;
};

TransactionService *transaction_service_new(TransactionRepository tx_repo,
                                            TxAccountRepository   account_repo)
{
    TransactionService *svc = (TransactionService *)malloc(sizeof *svc);
    if (!svc) return NULL;
    svc->tx_repo      = tx_repo;
    svc->account_repo = account_repo;
    return svc;
}

void transaction_service_free(TransactionService *svc) {
    free(svc);
}

/*  Internal helpers                                                     */

static void free_tx_array(Transaction **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; ++i) transaction_free(arr[i]);
    free(arr);
}

static void free_account_array(Account **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; ++i) account_free(arr[i]);
    free(arr);
}

static ServiceError translate_repo_error(const RepoError *rerr,
                                          const char *entity)
{
    if (!rerr || rerr->code == REPO_ERROR_NONE) return service_error_ok();
    if (rerr->code == REPO_ERROR_NOT_FOUND)     return service_error_not_found(entity);
    return service_error_from_repo(rerr);
}

static int32_t time_to_date_key(time_t t) {
    struct tm *tm_val = gmtime(&t);
    if (!tm_val) return 0;
    return (int32_t)((tm_val->tm_year + 1900) * 10000
                   + (tm_val->tm_mon  + 1)    * 100
                   +  tm_val->tm_mday);
}

/*  Thesis (§4.3.1): Rust's std::thread::available_parallelism() wraps   */
/*  these same OS primitives automatically.  In C we call them by hand   */
/*  and guard the platform split with #ifdef.                             */

static long get_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    long n = (long)info.dwNumberOfProcessors;
    return n > 0 ? n : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? n : 1;
#endif
}

/*  DayTotalMap: C equivalent of BTreeMap<NaiveDate, i64>               */

typedef struct DayTotalMap {
    DayTotal *entries;
    size_t    count;
    size_t    capacity;
} DayTotalMap;

static void day_total_map_init(DayTotalMap *m) {
    m->entries  = NULL;
    m->count    = 0;
    m->capacity = 0;
}

static void day_total_map_free(DayTotalMap *m) {
    free(m->entries);
    m->entries  = NULL;
    m->count    = 0;
    m->capacity = 0;
}

static bool day_total_map_add(DayTotalMap *m, int32_t date_key, int64_t amount) {
    for (size_t i = 0; i < m->count; ++i) {
        if (m->entries[i].date_key == date_key) {
            m->entries[i].total += amount;
            return true;
        }
    }
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity ? m->capacity * 2 : 16;
        DayTotal *buf  = (DayTotal *)realloc(m->entries,
                                              new_cap * sizeof(DayTotal));
        if (!buf) return false;
        m->entries  = buf;
        m->capacity = new_cap;
    }
    m->entries[m->count].date_key = date_key;
    m->entries[m->count].total    = amount;
    m->count++;
    return true;
}

static int compare_day_totals(const void *a, const void *b) {
    const DayTotal *da = (const DayTotal *)a;
    const DayTotal *db = (const DayTotal *)b;
    return (da->date_key > db->date_key) - (da->date_key < db->date_key);
}

/*  Worker thread shared state + args                                    */

typedef struct StatsShared {
    _Atomic int64_t   total_incoming;   /* §4.3.3 C11 atomic */
    _Atomic int64_t   total_outgoing;
    _Atomic int64_t   total_volume;

    pthread_mutex_t   per_type_mutex;   /* §4.3.2 mutex */
    PerTypeTotals     per_type;

    pthread_mutex_t   per_day_mutex;    /* §4.3.2 mutex */
    DayTotalMap       per_day;
} StatsShared;

typedef struct StatsWorkerArgs {
    Transaction **slice;
    size_t        count;
    StatsShared  *shared;

    /* Read-only view of the user's account ids; direction is decided relative
     * to this set (mirrors Rust's `user_account_ids` HashSet). */
    const int64_t *user_ids;
    size_t         user_count;

    /* Optional [from, to] window (mirrors Rust's Option<DateTime<Utc>>). */
    bool   has_from;
    time_t from;
    bool   has_to;
    time_t to;
} StatsWorkerArgs;

static bool id_in_set(const int64_t *ids, size_t count, int64_t value) {
    for (size_t i = 0; i < count; ++i) {
        if (ids[i] == value) return true;
    }
    return false;
}

static void *stats_worker_fn(void *raw) {
    StatsWorkerArgs *arg = (StatsWorkerArgs *)raw;
    StatsShared     *s   = arg->shared;

    /* Phase 1: thread-local accumulation — no locks in the hot path. */
    PerTypeTotals local_type;
    memset(&local_type, 0, sizeof local_type);

    DayTotalMap local_day;
    day_total_map_init(&local_day);

    for (size_t i = 0; i < arg->count; ++i) {
        const Transaction *tx    = arg->slice[i];

        /* Restrict to the requested [from, to] window. */
        time_t rec = transaction_recorded_on(tx);
        if (arg->has_from && rec < arg->from) continue;
        if (arg->has_to   && rec > arg->to)   continue;

        int64_t            value = transaction_value_cents(tx);
        TransactionType    type  = transaction_type_get(tx);
        int32_t            dkey  = time_to_date_key(rec);

        /* Signed effect on the user: credited when one of the user's accounts
         * is the destination, debited when one is the source. An internal
         * transfer between two owned accounts nets to zero. */
        bool to_owned   = id_in_set(arg->user_ids, arg->user_count,
                                    transaction_to_account_id(tx).value);
        bool from_owned = id_in_set(arg->user_ids, arg->user_count,
                                    transaction_from_account_id(tx).value);
        int64_t net = (to_owned ? value : 0) - (from_owned ? value : 0);

        if (net > 0) {
            /* §4.3.3: mirrors AtomicI64::fetch_add(v, Ordering::Relaxed) */
            atomic_fetch_add_explicit(&s->total_incoming, net,
                                      memory_order_relaxed);
        } else if (net < 0) {
            atomic_fetch_add_explicit(&s->total_outgoing, -net,
                                      memory_order_relaxed);
        }

        atomic_fetch_add_explicit(&s->total_volume, net < 0 ? -net : net,
                                  memory_order_relaxed);

        /* Per-type totals: gross value per type (once per unique tx). */
        if ((size_t)type < TX_SERVICE_TYPE_COUNT) {
            local_type.totals[type]  += value;
            local_type.present[type]  = true;
        }

        /* Per-day totals: SIGNED net so outgoing reduces the day's total
         * (the route derives daily spending from the negative part). */
        day_total_map_add(&local_day, dkey, net);
    }

    /* Phase 2: merge under mutex — one lock acquisition per thread per map. */

    /* §4.3.2: mirrors Arc<Mutex<HashMap>>::lock().unwrap() */
    pthread_mutex_lock(&s->per_type_mutex);
    for (size_t c = 0; c < TX_SERVICE_TYPE_COUNT; ++c) {
        if (local_type.present[c]) {
            s->per_type.totals[c]  += local_type.totals[c];
            s->per_type.present[c]  = true;
        }
    }
    pthread_mutex_unlock(&s->per_type_mutex);

    pthread_mutex_lock(&s->per_day_mutex);
    for (size_t d = 0; d < local_day.count; ++d) {
        day_total_map_add(&s->per_day,
                          local_day.entries[d].date_key,
                          local_day.entries[d].total);
    }
    pthread_mutex_unlock(&s->per_day_mutex);

    day_total_map_free(&local_day);
    return NULL;
}

/*  Comparators for statement sort                                       */

/* Sort transaction pointers oldest -> newest, ties broken by id ascending. */
static int compare_tx_by_recorded_on(const void *a, const void *b) {
    const Transaction *ta = *(const Transaction *const *)a;
    const Transaction *tb = *(const Transaction *const *)b;
    time_t ra = transaction_recorded_on(ta);
    time_t rb = transaction_recorded_on(tb);
    if (ra != rb) return (ra > rb) - (ra < rb);
    int64_t ia = transaction_id(ta).value;
    int64_t ib = transaction_id(tb).value;
    return (ia > ib) - (ia < ib);
}

/* Sort statement entries oldest -> newest (recorded_on, then id ascending). */
static int compare_entry_asc(const void *a, const void *b) {
    const AccountStatementEntry *ea = (const AccountStatementEntry *)a;
    const AccountStatementEntry *eb = (const AccountStatementEntry *)b;
    if (ea->recorded_on != eb->recorded_on)
        return (ea->recorded_on > eb->recorded_on) - (ea->recorded_on < eb->recorded_on);
    int64_t ia = ea->transaction_id.value;
    int64_t ib = eb->transaction_id.value;
    return (ia > ib) - (ia < ib);
}

/* Sort statement entries newest -> oldest (reverse of compare_entry_asc). */
static int compare_entry_desc(const void *a, const void *b) {
    return compare_entry_asc(b, a);
}

/*
 * Apply a transaction's monetary effect to the affected account balances and
 * persist them. Mirrors the `match cmd.transaction_type` block in Rust's
 * record_transaction (same signs, same overdraft rule, same domain helpers).
 */
static bool record_transaction_apply_balances(
    TransactionService *svc,
    const RecordTransactionCommand *cmd,
    ServiceError *err)
{
    RepoError   rerr;
    DomainError derr;

    switch (cmd->transaction_type) {
    case TRANSACTION_TYPE_DEPOSIT: {
        Account *to_acc = NULL;
        if (!svc->account_repo.vtable->get_by_id(
                svc->account_repo.ctx, cmd->to_account_id, &to_acc, &rerr)) {
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (!account_credit(to_acc, cmd->value_cents, &derr)) {
            account_free(to_acc);
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }
        if (!svc->account_repo.vtable->update(svc->account_repo.ctx, to_acc, &rerr)) {
            account_free(to_acc);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        account_free(to_acc);
        break;
    }

    case TRANSACTION_TYPE_WITHDRAWAL:
    case TRANSACTION_TYPE_PAYMENT: {
        Account *from_acc = NULL;
        if (!svc->account_repo.vtable->get_by_id(
                svc->account_repo.ctx, cmd->from_account_id, &from_acc, &rerr)) {
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (account_balance_cents(from_acc) < cmd->value_cents) {
            account_free(from_acc);
            if (err) *err = service_error_validation("insufficient funds");
            return false;
        }
        if (!account_debit(from_acc, cmd->value_cents, &derr)) {
            account_free(from_acc);
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }
        if (!svc->account_repo.vtable->update(svc->account_repo.ctx, from_acc, &rerr)) {
            account_free(from_acc);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        account_free(from_acc);
        break;
    }

    case TRANSACTION_TYPE_SEND:
    case TRANSACTION_TYPE_TRANSFER: {
        Account *from_acc = NULL, *to_acc = NULL;
        if (!svc->account_repo.vtable->get_by_id(
                svc->account_repo.ctx, cmd->from_account_id, &from_acc, &rerr)) {
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (!svc->account_repo.vtable->get_by_id(
                svc->account_repo.ctx, cmd->to_account_id, &to_acc, &rerr)) {
            account_free(from_acc);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (account_balance_cents(from_acc) < cmd->value_cents) {
            account_free(from_acc);
            account_free(to_acc);
            if (err) *err = service_error_validation("insufficient funds");
            return false;
        }
        if (!account_debit(from_acc, cmd->value_cents, &derr)) {
            account_free(from_acc);
            account_free(to_acc);
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }
        if (!account_credit(to_acc, cmd->value_cents, &derr)) {
            account_free(from_acc);
            account_free(to_acc);
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }
        if (!svc->account_repo.vtable->update(svc->account_repo.ctx, from_acc, &rerr)) {
            account_free(from_acc);
            account_free(to_acc);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (!svc->account_repo.vtable->update(svc->account_repo.ctx, to_acc, &rerr)) {
            account_free(from_acc);
            account_free(to_acc);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        account_free(from_acc);
        account_free(to_acc);
        break;
    }

    default:
        break;
    }

    return true;
}

/*  Public methods                                                       */

bool transaction_service_record_transaction(
    TransactionService *svc,
    const RecordTransactionCommand *cmd,
    TransactionId *out_id,
    ServiceError *err)
{
    if (!svc || !cmd || !out_id) {
        if (err) *err = service_error_validation(
            "TransactionService::record_transaction: invalid arguments");
        return false;
    }

    /* §4.1 Type-Driven Invariants: value > 0 enforced before domain call. */
    if (cmd->value_cents <= 0) {
        if (err) *err = service_error_validation(
            "transaction value must be strictly positive");
        return false;
    }

    DomainError derr;
    Transaction *tx = transaction_create(
        cmd->from_account_id,
        cmd->to_account_id,
        cmd->transaction_type,
        cmd->value_cents,
        cmd->description,
        &derr
    );
    if (!tx) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    /*
     * Apply the monetary effect to the affected account balances using the
     * domain helpers, then persist it — mirroring Rust's record_transaction.
     *   Deposit            -> credit destination
     *   Withdrawal/Payment -> debit source (reject overdraft)
     *   Send/Transfer      -> debit source AND credit destination
     * Any operation that would overdraw the source is rejected with a
     * validation error ("insufficient funds") before the row is written.
     */
    if (!record_transaction_apply_balances(svc, cmd, err)) {
        transaction_free(tx);
        return false;
    }

    RepoError rerr;
    TransactionId new_id;
    bool ok = svc->tx_repo.vtable->insert(
        svc->tx_repo.ctx, tx, &new_id, &rerr);

    transaction_free(tx);

    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    *out_id = new_id;
    if (err) *err = service_error_ok();
    return true;
}

bool transaction_service_list_for_account(
    TransactionService *svc,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    ServiceError *err)
{
    if (!svc || !out_txs || !out_count) {
        if (err) *err = service_error_validation(
            "TransactionService::list_for_account: invalid arguments");
        return false;
    }

    /* Mirrors Rust: limit.clamp(1, 100), offset.max(0). */
    if (limit < 1)   limit = 1;
    if (limit > 100) limit = 100;
    if (offset < 0)  offset = 0;

    RepoError rerr;
    bool ok = svc->tx_repo.vtable->list_for_account(
        svc->tx_repo.ctx, account_id, limit, offset, out_txs, out_count, &rerr);

    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}

bool transaction_service_compute_account_statement(
    TransactionService *svc,
    const AccountStatementQuery *query,
    AccountStatementEntry **out_entries,
    size_t *out_count,
    ServiceError *err)
{
    if (!svc || !query || !out_entries || !out_count) {
        if (err) *err = service_error_validation(
            "TransactionService::compute_account_statement: invalid arguments");
        return false;
    }

    AccountId account_id = query->account_id;

    /*
     * 1. Load the account's CURRENT balance. It reflects every transaction ever
     *    applied, so it anchors the running balance: opening balance is
     *    reconstructed as `current - sum(all signed deltas)`.
     */
    Account  *account = NULL;
    RepoError rerr;
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, account_id, &account, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    int64_t current_balance = account_balance_cents(account);
    account_free(account);

    /*
     * 2. Load the FULL history (INT64_MAX limit). Date filtering and pagination
     *    happen *after* the running-balance computation, so a date-range query
     *    is never truncated by an early LIMIT and pagination slices the correct,
     *    already-filtered set.
     */
    Transaction **txs   = NULL;
    size_t        count = 0;
    if (!svc->tx_repo.vtable->list_for_account(
            svc->tx_repo.ctx, account_id, INT64_MAX, 0, &txs, &count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* 3. Chronological order (oldest -> newest), ties broken by id. */
    if (count > 1) {
        qsort(txs, count, sizeof(Transaction *), compare_tx_by_recorded_on);
    }

    /* 4. Reconstruct opening balance: opening = current - sum(signed deltas).
     *    Signed delta: credit when the account is the destination (`to`),
     *    debit when it is the source (`from`). */
    int64_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t val = transaction_value_cents(txs[i]);
        total += (transaction_to_account_id(txs[i]).value == account_id.value)
                 ? val : -val;
    }
    int64_t running = current_balance - total;

    /* 5. Forward pass: tag every transaction with its balance-after. */
    AccountStatementEntry *entries = NULL;
    if (count > 0) {
        entries = (AccountStatementEntry *)calloc(
            count, sizeof(AccountStatementEntry));
        if (!entries) {
            free_tx_array(txs, count);
            if (err) *err = service_error_validation(
                "compute_account_statement: out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        const Transaction *tx   = txs[i];
        int64_t            val  = transaction_value_cents(tx);
        int64_t            delta = (transaction_to_account_id(tx).value == account_id.value)
                                   ? val : -val;
        running += delta;

        entries[i].transaction_id      = transaction_id(tx);
        entries[i].recorded_on         = transaction_recorded_on(tx);
        entries[i].transaction_type    = transaction_type_get(tx);
        entries[i].value_cents         = val;
        entries[i].balance_after_cents = running;

        const char *desc = transaction_description(tx);
        if (desc) {
            strncpy(entries[i].description, desc,
                    sizeof(entries[i].description) - 1);
            entries[i].description[sizeof(entries[i].description) - 1] = '\0';
        }
    }

    free_tx_array(txs, count);

    /* 6. Date-range filter (compact in-place), applied before pagination. */
    if (query->has_from || query->has_to) {
        size_t write = 0;
        for (size_t i = 0; i < count; ++i) {
            time_t rec  = entries[i].recorded_on;
            bool   keep = true;
            if (query->has_from && rec < query->from) keep = false;
            if (query->has_to   && rec > query->to)   keep = false;
            if (keep) entries[write++] = entries[i];
        }
        count = write;
    }

    /*
     * 7. Pagination mirrors the original "most recent first" semantics:
     *    order newest -> oldest, take the requested [offset, offset+limit)
     *    slice, then present it oldest -> newest. Each entry keeps its
     *    absolute balance-after computed over the full history.
     */
    if (count > 1) {
        qsort(entries, count, sizeof(AccountStatementEntry), compare_entry_desc);
    }

    size_t off = (query->offset < 0) ? 0 : (size_t)query->offset;
    size_t lim = (query->limit  < 0) ? 0 : (size_t)query->limit;

    size_t start = (off < count) ? off : count;
    size_t n     = count - start;
    if (n > lim) n = lim;

    if (n == 0) {
        free(entries);
        *out_entries = NULL;
        *out_count   = 0;
        if (err) *err = service_error_ok();
        return true;
    }

    if (start > 0) {
        memmove(entries, entries + start, n * sizeof(AccountStatementEntry));
    }
    count = n;

    if (count > 1) {
        qsort(entries, count, sizeof(AccountStatementEntry), compare_entry_asc);
    }

    *out_entries = entries;
    *out_count   = count;
    if (err) *err = service_error_ok();
    return true;
}

bool transaction_service_compute_user_statistics(
    TransactionService *svc,
    UserId user_id,
    int64_t per_account_limit,
    bool has_from,
    time_t from,
    bool has_to,
    time_t to,
    UserTransactionStatistics *out,
    ServiceError *err)
{
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "TransactionService::compute_user_statistics: invalid arguments");
        return false;
    }

    /* Phase A: I/O — load all accounts. */
    Account **accounts      = NULL;
    size_t    account_count = 0;
    RepoError rerr;

    if (!svc->account_repo.vtable->list_for_user(
            svc->account_repo.ctx, user_id,
            &accounts, &account_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* The set of account ids owned by this user. Direction (incoming vs
     * outgoing) is decided relative to this set, not by transaction type. */
    int64_t *user_ids = NULL;
    if (account_count > 0) {
        user_ids = (int64_t *)calloc(account_count, sizeof(int64_t));
        if (!user_ids) {
            free_account_array(accounts, account_count);
            if (err) *err = service_error_validation(
                "compute_user_statistics: out of memory");
            return false;
        }
        for (size_t a = 0; a < account_count; ++a) {
            user_ids[a] = account_id(accounts[a]).value;
        }
    }

    /* Phase B: I/O — gather all transactions into one flat array. */
    Transaction **all_txs   = NULL;
    size_t        all_count = 0;
    size_t        all_cap   = 0;

    for (size_t a = 0; a < account_count; ++a) {
        AccountId     aid   = account_id(accounts[a]);
        Transaction **chunk = NULL;
        size_t        chunk_count = 0;

        bool ok = svc->tx_repo.vtable->list_for_account(
            svc->tx_repo.ctx, aid,
            per_account_limit, 0,
            &chunk, &chunk_count, &rerr);

        if (!ok) {
            free(user_ids);
            free_account_array(accounts, account_count);
            free_tx_array(all_txs, all_count);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }

        if (all_count + chunk_count > all_cap) {
            size_t new_cap = (all_cap + chunk_count) * 2;
            Transaction **buf = (Transaction **)realloc(
                all_txs, new_cap * sizeof(Transaction *));
            if (!buf) {
                free(chunk);
                free(user_ids);
                free_account_array(accounts, account_count);
                free_tx_array(all_txs, all_count);
                if (err) *err = service_error_validation(
                    "compute_user_statistics: out of memory");
                return false;
            }
            all_txs = buf;
            all_cap = new_cap;
        }

        memcpy(all_txs + all_count, chunk, chunk_count * sizeof(Transaction *));
        all_count += chunk_count;
        free(chunk);   /* free the pointer array only, not the Transactions */
    }

    free_account_array(accounts, account_count);

    /* DEDUPLICATE by transaction id. An internal Transfer/Send between two of
     * the user's own accounts is returned once per side; keeping it once
     * prevents double counting (mirrors Rust's HashMap<TransactionId, _>). */
    {
        size_t w = 0;
        for (size_t i = 0; i < all_count; ++i) {
            int64_t id  = transaction_id(all_txs[i]).value;
            bool    dup = false;
            for (size_t j = 0; j < w; ++j) {
                if (transaction_id(all_txs[j]).value == id) { dup = true; break; }
            }
            if (dup) {
                transaction_free(all_txs[i]);
            } else {
                all_txs[w++] = all_txs[i];
            }
        }
        all_count = w;
    }

    /* Phase C: CPU — parallel aggregation across all transactions.
     *
     * Mirrors Rust:
     *   let num_threads = thread::available_parallelism()
     *       .map(|n| n.get()).unwrap_or(1);
     */
    long   nproc       = get_processor_count();
    size_t num_threads = (nproc > 0) ? (size_t)nproc : 1;
    if (all_count > 0 && num_threads > all_count) num_threads = all_count;
    if (num_threads == 0) num_threads = 1;

    size_t chunk_size = all_count / num_threads;
    if (chunk_size == 0) chunk_size = 1;

    /* Initialise shared state. */

    /* §4.3.3: mirrors AtomicI64::new(0) */
    StatsShared shared;
    atomic_init(&shared.total_incoming, 0);
    atomic_init(&shared.total_outgoing, 0);
    atomic_init(&shared.total_volume,   0);

    /* §4.3.2: mirrors Arc::new(Mutex::new(HashMap::new())) */
    pthread_mutex_init(&shared.per_type_mutex, NULL);
    pthread_mutex_init(&shared.per_day_mutex,  NULL);
    memset(&shared.per_type, 0, sizeof shared.per_type);
    day_total_map_init(&shared.per_day);

    StatsWorkerArgs *args    = (StatsWorkerArgs *)calloc(
        num_threads, sizeof *args);
    pthread_t       *threads = (pthread_t *)calloc(
        num_threads, sizeof *threads);

    if (!args || !threads) {
        free(args); free(threads);
        pthread_mutex_destroy(&shared.per_type_mutex);
        pthread_mutex_destroy(&shared.per_day_mutex);
        day_total_map_free(&shared.per_day);
        free_tx_array(all_txs, all_count);
        free(user_ids);
        if (err) *err = service_error_validation(
            "compute_user_statistics: out of memory");
        return false;
    }

    /*
     * §4.3.1: spawn workers.
     * Mirrors:
     *   thread::scope(|scope| {
     *       for chunk in all_transactions.chunks(chunk_size) {
     *           scope.spawn(move || { ... });
     *       }
     *   });
     */
    size_t offset = 0;
    for (size_t t = 0; t < num_threads; ++t) {
        size_t slice = (t == num_threads - 1)
                       ? (all_count - offset)
                       :  chunk_size;

        args[t].slice      = all_txs + offset;
        args[t].count      = slice;
        args[t].shared     = &shared;
        args[t].user_ids   = user_ids;
        args[t].user_count = account_count;
        args[t].has_from   = has_from;
        args[t].from       = from;
        args[t].has_to     = has_to;
        args[t].to         = to;

        pthread_create(&threads[t], NULL, stats_worker_fn, &args[t]);
        offset += slice;
    }

    /* Join all threads — mirrors scope dropping in Rust. */
    for (size_t t = 0; t < num_threads; ++t) {
        pthread_join(threads[t], NULL);
    }

    free(args);
    free(threads);
    free_tx_array(all_txs, all_count);
    free(user_ids);

    /* Phase D: collect results. */

    /* §4.3.3: mirrors AtomicI64::load(Ordering::Relaxed) */
    out->total_incoming_cents =
        atomic_load_explicit(&shared.total_incoming, memory_order_relaxed);
    out->total_outgoing_cents =
        atomic_load_explicit(&shared.total_outgoing, memory_order_relaxed);
    out->total_volume_cents   =
        atomic_load_explicit(&shared.total_volume,   memory_order_relaxed);

    out->per_type = shared.per_type;

    /* Sort per_day — BTreeMap in Rust is always sorted; we must do this explicitly. */
    if (shared.per_day.count > 1) {
        qsort(shared.per_day.entries,
              shared.per_day.count,
              sizeof(DayTotal),
              compare_day_totals);
    }

    /* Transfer ownership of the per_day array to the caller. */
    out->per_day       = shared.per_day.entries;
    out->per_day_count = shared.per_day.count;
    /* Do NOT call day_total_map_free — ownership transferred above. */

    pthread_mutex_destroy(&shared.per_type_mutex);
    pthread_mutex_destroy(&shared.per_day_mutex);

    if (err) *err = service_error_ok();
    return true;
}

void user_transaction_statistics_free(UserTransactionStatistics *stats) {
    if (!stats) return;
    free(stats->per_day);
    stats->per_day       = NULL;
    stats->per_day_count = 0;
}

/*  User-facing use-cases (business logic; never in the server layer)    */

/*
 * ensure_account_owned_by: load the account and confirm it belongs to the
 * given user. A missing account or a mismatch both map to NotFound("account"),
 * exactly like Rust's `ensure_account_owned_by`.
 */
static bool ensure_account_owned_by(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    ServiceError *err)
{
    Account  *account = NULL;
    RepoError rerr;

    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, account_id, &account, &rerr)) {
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }

    bool owned = (account_user_id(account).value == user_id.value);
    account_free(account);

    if (!owned) {
        if (err) *err = service_error_not_found("account");
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}

/* Convert a civil UTC date+time to time_t without relying on timegm/_mkgmtime
 * (portable across MSYS2 / Windows). Uses the well-known days-from-civil
 * algorithm so the [from, to] bounds match Rust's UTC interpretation. */
static time_t utc_civil_to_time(int year, int month, int day,
                                int hour, int min, int sec) {
    /* days_from_civil (Howard Hinnant's algorithm) */
    int y = year;
    y -= (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = (long long)era * 146097 + (long long)doe - 719468;
    return (time_t)(days * 86400 + hour * 3600 + min * 60 + sec);
}

/* Parse "YYYY-MM-DD"; on success fill the date fields. */
static bool parse_ymd(const char *s, int *y, int *m, int *d) {
    if (!s) return false;
    return sscanf(s, "%d-%d-%d", y, m, d) == 3;
}

static bool record_deposit(TransactionService *svc, AccountId account_id,
                           int64_t amount_units, TransactionId *out_id,
                           ServiceError *err) {
    if (amount_units <= 0) {
        if (err) *err = service_error_validation("deposit amount must be positive");
        return false;
    }
    if (amount_units > INT64_MAX / 100) {
        if (err) *err = service_error_validation("amount too large");
        return false;
    }
    RecordTransactionCommand cmd = {
        .from_account_id  = BANK_ACCOUNT_ID,
        .to_account_id    = account_id,
        .transaction_type = TRANSACTION_TYPE_DEPOSIT,
        .value_cents      = amount_units * 100,
        .description      = "ATM Deposit",
    };
    return transaction_service_record_transaction(svc, &cmd, out_id, err);
}

static bool record_withdrawal(TransactionService *svc, AccountId account_id,
                              int64_t amount_units, TransactionId *out_id,
                              ServiceError *err) {
    if (amount_units <= 0) {
        if (err) *err = service_error_validation("withdrawal amount must be positive");
        return false;
    }
    if (amount_units > INT64_MAX / 100) {
        if (err) *err = service_error_validation("amount too large");
        return false;
    }
    RecordTransactionCommand cmd = {
        .from_account_id  = account_id,
        .to_account_id    = BANK_ACCOUNT_ID,
        .transaction_type = TRANSACTION_TYPE_WITHDRAWAL,
        .value_cents      = amount_units * 100,
        .description      = "ATM Withdrawal",
    };
    return transaction_service_record_transaction(svc, &cmd, out_id, err);
}

static bool record_send(TransactionService *svc, AccountId from_account_id,
                        AccountId recipient_account_id, int64_t value_cents,
                        const char *message, TransactionId *out_id,
                        ServiceError *err) {
    if (value_cents <= 0) {
        if (err) *err = service_error_validation("send amount must be positive");
        return false;
    }

    Account  *from_acc = NULL, *to_acc = NULL;
    RepoError rerr;

    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, from_account_id, &from_acc, &rerr)) {
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, recipient_account_id, &to_acc, &rerr)) {
        account_free(from_acc);
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }

    bool same_currency = (account_currency(from_acc) == account_currency(to_acc));
    account_free(from_acc);
    account_free(to_acc);

    if (!same_currency) {
        if (err) *err = service_error_validation(
            "cannot send between accounts with different currencies (use exchange)");
        return false;
    }

    /* description = "Send: {trimmed message}" */
    char desc[256];
    const char *m = message ? message : "";
    while (*m == ' ' || *m == '\t' || *m == '\n' || *m == '\r') ++m;
    size_t mlen = strlen(m);
    while (mlen > 0 && (m[mlen-1] == ' ' || m[mlen-1] == '\t' ||
                        m[mlen-1] == '\n' || m[mlen-1] == '\r')) --mlen;
    snprintf(desc, sizeof desc, "Send: %.*s", (int)mlen, m);

    RecordTransactionCommand cmd = {
        .from_account_id  = from_account_id,
        .to_account_id    = recipient_account_id,
        .transaction_type = TRANSACTION_TYPE_SEND,
        .value_cents      = value_cents,
        .description      = desc,
    };
    return transaction_service_record_transaction(svc, &cmd, out_id, err);
}

static bool record_transfer(TransactionService *svc, AccountId from_account_id,
                            AccountId to_account_id, int64_t value_cents,
                            TransactionId *out_id, ServiceError *err) {
    if (value_cents <= 0) {
        if (err) *err = service_error_validation("transfer amount must be positive");
        return false;
    }

    Account  *from_acc = NULL, *to_acc = NULL;
    RepoError rerr;

    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, from_account_id, &from_acc, &rerr)) {
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, to_account_id, &to_acc, &rerr)) {
        account_free(from_acc);
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }

    bool same_user     = (account_user_id(from_acc).value == account_user_id(to_acc).value);
    bool same_currency = (account_currency(from_acc) == account_currency(to_acc));

    char desc[256];
    snprintf(desc, sizeof desc, "Transfer %s %s -> %s %s",
             account_type_as_str(account_type_get(from_acc)),
             currency_as_str(account_currency(from_acc)),
             account_type_as_str(account_type_get(to_acc)),
             currency_as_str(account_currency(to_acc)));

    account_free(from_acc);
    account_free(to_acc);

    if (!same_user) {
        if (err) *err = service_error_forbidden();
        return false;
    }
    if (!same_currency) {
        if (err) *err = service_error_validation(
            "cross-currency transfer not supported yet");
        return false;
    }

    RecordTransactionCommand cmd = {
        .from_account_id  = from_account_id,
        .to_account_id    = to_account_id,
        .transaction_type = TRANSACTION_TYPE_TRANSFER,
        .value_cents      = value_cents,
        .description      = desc,
    };
    return transaction_service_record_transaction(svc, &cmd, out_id, err);
}

/* Trim helper into a fixed buffer. */
static void trim_into(char *dst, size_t dst_size, const char *src) {
    const char *s = src ? src : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') ++s;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) --len;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, s, len);
    dst[len] = '\0';
}

static bool record_payment(TransactionService *svc, AccountId from_account_id,
                           int64_t amount_units, const char *category,
                           const char *merchant_name, const char *note_opt,
                           TransactionId *out_id, ServiceError *err) {
    if (amount_units <= 0) {
        if (err) *err = service_error_validation("payment amount must be positive");
        return false;
    }
    if (amount_units > INT64_MAX / 100) {
        if (err) *err = service_error_validation("amount too large");
        return false;
    }

    char cat[96], merch[96], note[96];
    trim_into(cat,   sizeof cat,   category);
    trim_into(merch, sizeof merch, merchant_name);

    char desc[256];
    if (note_opt) {
        trim_into(note, sizeof note, note_opt);
        snprintf(desc, sizeof desc,
                 "Payment | category: %s | merchant: %s | note: %s",
                 cat, merch, note);
    } else {
        snprintf(desc, sizeof desc,
                 "Payment | category: %s | merchant: %s",
                 cat, merch);
    }

    RecordTransactionCommand cmd = {
        .from_account_id  = from_account_id,
        .to_account_id    = BANK_ACCOUNT_ID,
        .transaction_type = TRANSACTION_TYPE_PAYMENT,
        .value_cents      = amount_units * 100,
        .description      = desc,
    };
    return transaction_service_record_transaction(svc, &cmd, out_id, err);
}

bool transaction_service_record_deposit_for_user(
    TransactionService *svc, UserId user_id, AccountId account_id,
    int64_t amount_units, TransactionId *out_id, ServiceError *err)
{
    if (!svc || !out_id) {
        if (err) *err = service_error_validation("record_deposit_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;
    return record_deposit(svc, account_id, amount_units, out_id, err);
}

bool transaction_service_record_withdrawal_for_user(
    TransactionService *svc, UserId user_id, AccountId account_id,
    int64_t amount_units, TransactionId *out_id, ServiceError *err)
{
    if (!svc || !out_id) {
        if (err) *err = service_error_validation("record_withdrawal_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;
    return record_withdrawal(svc, account_id, amount_units, out_id, err);
}

bool transaction_service_record_send_for_user(
    TransactionService *svc, UserId user_id, AccountId from_account_id,
    AccountId recipient_account_id, int64_t value_cents, const char *message,
    TransactionId *out_id, ServiceError *err)
{
    if (!svc || !out_id) {
        if (err) *err = service_error_validation("record_send_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, from_account_id, err)) return false;
    return record_send(svc, from_account_id, recipient_account_id,
                       value_cents, message, out_id, err);
}

bool transaction_service_record_transfer_for_user(
    TransactionService *svc, UserId user_id, AccountId from_account_id,
    AccountId to_account_id, int64_t value_cents,
    TransactionId *out_id, ServiceError *err)
{
    if (!svc || !out_id) {
        if (err) *err = service_error_validation("record_transfer_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, from_account_id, err)) return false;
    if (!ensure_account_owned_by(svc, user_id, to_account_id, err))   return false;
    return record_transfer(svc, from_account_id, to_account_id, value_cents, out_id, err);
}

bool transaction_service_record_payment_for_user(
    TransactionService *svc, UserId user_id, AccountId from_account_id,
    int64_t amount_units, const char *category, const char *merchant_name,
    const char *note_opt, TransactionId *out_id, ServiceError *err)
{
    if (!svc || !out_id) {
        if (err) *err = service_error_validation("record_payment_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, from_account_id, err)) return false;
    return record_payment(svc, from_account_id, amount_units,
                          category, merchant_name, note_opt, out_id, err);
}

bool transaction_service_list_recent_for_user(
    TransactionService *svc, UserId user_id, AccountId account_id,
    int64_t limit, Transaction ***out_txs, size_t *out_count, ServiceError *err)
{
    if (!svc || !out_txs || !out_count) {
        if (err) *err = service_error_validation("list_recent_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;

    /* Mirrors Rust: limit.unwrap_or(10).max(1). */
    if (limit <= 0) limit = 10;
    if (limit < 1)  limit = 1;

    return transaction_service_list_for_account(
        svc, account_id, limit, 0, out_txs, out_count, err);
}

bool transaction_service_compute_account_statement_for_user_from_strings(
    TransactionService *svc, UserId user_id, AccountId account_id,
    const char *from_opt, const char *to_opt, int64_t limit, int64_t offset,
    AccountStatementEntry **out_entries, size_t *out_count, ServiceError *err)
{
    if (!svc || !out_entries || !out_count) {
        if (err) *err = service_error_validation(
            "compute_account_statement_for_user_from_strings: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;

    AccountStatementQuery query;
    query.account_id = account_id;
    query.has_from   = false;
    query.from       = 0;
    query.has_to     = false;
    query.to         = 0;
    query.limit      = (limit <= 0)  ? 100 : limit;
    query.offset     = (offset < 0)  ? 0   : offset;

    if (from_opt) {
        int y, m, d;
        if (!parse_ymd(from_opt, &y, &m, &d)) {
            if (err) *err = service_error_validation("invalid 'from' date (expected YYYY-MM-DD)");
            return false;
        }
        query.has_from = true;
        query.from     = utc_civil_to_time(y, m, d, 0, 0, 0);
    }
    if (to_opt) {
        int y, m, d;
        if (!parse_ymd(to_opt, &y, &m, &d)) {
            if (err) *err = service_error_validation("invalid 'to' date (expected YYYY-MM-DD)");
            return false;
        }
        query.has_to = true;
        query.to     = utc_civil_to_time(y, m, d, 23, 59, 59);
    }

    return transaction_service_compute_account_statement(
        svc, &query, out_entries, out_count, err);
}