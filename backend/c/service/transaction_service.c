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

static const TxAccountRepositoryVTable TX_ACCT_REPO_VTABLE = {
    tx_acct_repo_list_for_user_adapter
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
} StatsWorkerArgs;

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
        int64_t            value = transaction_value_cents(tx);
        TransactionType    type  = transaction_type_get(tx);
        int32_t            dkey  = time_to_date_key(transaction_recorded_on(tx));

        switch (type) {
        case TRANSACTION_TYPE_TRANSFER:
        case TRANSACTION_TYPE_SEND:
        case TRANSACTION_TYPE_WITHDRAWAL:
            /* §4.3.3: mirrors AtomicI64::fetch_add(v, Ordering::Relaxed) */
            atomic_fetch_add_explicit(&s->total_outgoing, value,
                                      memory_order_relaxed);
            break;
        case TRANSACTION_TYPE_DEPOSIT:
            atomic_fetch_add_explicit(&s->total_incoming, value,
                                      memory_order_relaxed);
            break;
        default:
            break;
        }

        atomic_fetch_add_explicit(&s->total_volume, value,
                                  memory_order_relaxed);

        if ((size_t)type < TX_SERVICE_TYPE_COUNT) {
            local_type.totals[type]  += value;
            local_type.present[type]  = true;
        }

        day_total_map_add(&local_day, dkey, value);
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

/*  Comparator for statement sort (oldest -> newest)                     */

static int compare_tx_by_recorded_on(const void *a, const void *b) {
    const Transaction *ta = *(const Transaction *const *)a;
    const Transaction *tb = *(const Transaction *const *)b;
    time_t ra = transaction_recorded_on(ta);
    time_t rb = transaction_recorded_on(tb);
    return (ra > rb) - (ra < rb);
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

    /* 1. Load transactions (I/O). */
    Transaction **txs   = NULL;
    size_t        count = 0;
    RepoError     rerr;

    bool ok = svc->tx_repo.vtable->list_for_account(
        svc->tx_repo.ctx,
        query->account_id,
        query->limit,
        query->offset,
        &txs,
        &count,
        &rerr
    );
    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /*
     * 2. Filter by optional time range — compact in-place.
     * Mirrors Vec::retain in Rust.
     */
    if (query->has_from || query->has_to) {
        size_t write = 0;
        for (size_t i = 0; i < count; ++i) {
            time_t rec = transaction_recorded_on(txs[i]);
            bool keep  = true;
            if (query->has_from && rec < query->from) keep = false;
            if (query->has_to   && rec > query->to)   keep = false;
            if (keep) {
                txs[write++] = txs[i];
            } else {
                transaction_free(txs[i]);
            }
        }
        count = write;
    }

    /* 3. Sort oldest -> newest. Mirrors txs.sort_by_key(|t| t.recorded_on()). */
    if (count > 1) {
        qsort(txs, count, sizeof(Transaction *), compare_tx_by_recorded_on);
    }

    /* 4. Compute running balance. */
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

    int64_t balance = 0;
    for (size_t i = 0; i < count; ++i) {
        const Transaction *tx   = txs[i];
        int64_t            val  = transaction_value_cents(tx);
        TransactionType    type = transaction_type_get(tx);

        int64_t delta;
        switch (type) {
        case TRANSACTION_TYPE_TRANSFER:
        case TRANSACTION_TYPE_WITHDRAWAL:
        case TRANSACTION_TYPE_SEND:
            delta = -val;
            break;
        case TRANSACTION_TYPE_DEPOSIT:
        default:
            delta = val;
            break;
        }
        balance += delta;

        entries[i].transaction_id      = transaction_id(tx);
        entries[i].recorded_on         = transaction_recorded_on(tx);
        entries[i].transaction_type    = type;
        entries[i].value_cents         = val;
        entries[i].balance_after_cents = balance;

        const char *desc = transaction_description(tx);
        if (desc) {
            strncpy(entries[i].description, desc,
                    sizeof(entries[i].description) - 1);
            entries[i].description[sizeof(entries[i].description) - 1] = '\0';
        }
    }

    free_tx_array(txs, count);

    *out_entries = entries;
    *out_count   = count;
    if (err) *err = service_error_ok();
    return true;
}

bool transaction_service_compute_user_statistics(
    TransactionService *svc,
    UserId user_id,
    int64_t per_account_limit,
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

        args[t].slice  = all_txs + offset;
        args[t].count  = slice;
        args[t].shared = &shared;

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