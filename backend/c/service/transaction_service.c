#include "include/transaction_service.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <unistd.h>
#endif

// System bank account for deposits/withdrawals/payments (Rust BANK_ACCOUNT_ID).
static const AccountId BANK_ACCOUNT_ID = { 1 };

// Sort by id, then scan-dedupe — O(n log n) vs O(n²) nested loop.
static int compare_tx_ptr_by_id(const void *a, const void *b) {
    const Transaction * const *ta = (const Transaction * const *)a;
    const Transaction * const *tb = (const Transaction * const *)b;
    int64_t ida = transaction_id(*ta).value;
    int64_t idb = transaction_id(*tb).value;
    if (ida < idb) return -1;
    if (ida > idb) return 1;
    return 0;
}

static size_t dedupe_transactions_by_id(Transaction **all_txs, size_t all_count) {
    if (all_count <= 1) return all_count;

    qsort(all_txs, all_count, sizeof(Transaction *), compare_tx_ptr_by_id);

    size_t w = 1;
    for (size_t i = 1; i < all_count; ++i) {
        if (transaction_id(all_txs[i]).value == transaction_id(all_txs[w - 1]).value) {
            transaction_free(all_txs[i]);
        } else {
            all_txs[w++] = all_txs[i];
        }
    }
    return w;
}

// Repo adapters (DB -> abstract interfaces).

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

static bool tx_repo_begin_adapter(void *ctx, RepoError *err) {
    return transaction_repo_begin((TransactionRepo *)ctx, err);
}

static bool tx_repo_commit_adapter(void *ctx, RepoError *err) {
    return transaction_repo_commit((TransactionRepo *)ctx, err);
}

static bool tx_repo_rollback_adapter(void *ctx) {
    return transaction_repo_rollback((TransactionRepo *)ctx);
}

static const TransactionRepositoryVTable TX_REPO_VTABLE = {
    tx_repo_get_by_id_adapter,
    tx_repo_insert_adapter,
    tx_repo_list_for_account_adapter,
    tx_repo_begin_adapter,
    tx_repo_commit_adapter,
    tx_repo_rollback_adapter
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
    struct tm tm_val;
#ifdef _WIN32
    if (gmtime_s(&tm_val, &t) != 0) return 0;
#else
    if (!gmtime_r(&t, &tm_val)) return 0;
#endif
    return (int32_t)((tm_val.tm_year + 1900) * 10000
                   + (tm_val.tm_mon  + 1)    * 100
                   +  tm_val.tm_mday);
}

// Cross-platform processor count (Rust available_parallelism parity).
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

typedef struct StatsShared {
    _Atomic int64_t   total_incoming;
    _Atomic int64_t   total_outgoing;
    _Atomic int64_t   total_volume;
    _Atomic int64_t   transaction_count;

    pthread_mutex_t   per_type_mutex;
    PerTypeTotals     per_type;

    pthread_mutex_t   per_day_mutex;
    DayTotalMap       per_day;
} StatsShared;

typedef struct StatsWorkerArgs {
    Transaction **slice;
    size_t        count;
    StatsShared  *shared;

    const int64_t *user_ids;
    size_t         user_count;

    bool   has_from;
    time_t from;
    bool   has_to;
    time_t to;

    bool    has_scope_account;
    int64_t scope_account_id;
    bool    has_tx_type;
    TransactionType filter_type;
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

    // 1. Thread-local accumulation (no locks in hot path).
    PerTypeTotals local_type;
    memset(&local_type, 0, sizeof local_type);

    DayTotalMap local_day;
    day_total_map_init(&local_day);

    for (size_t i = 0; i < arg->count; ++i) {
        const Transaction *tx    = arg->slice[i];

        time_t rec = transaction_recorded_on(tx);
        if (arg->has_from && rec < arg->from) continue;
        if (arg->has_to   && rec > arg->to)   continue;

        if (arg->has_tx_type && transaction_type_get(tx) != arg->filter_type) {
            continue;
        }

        int64_t            value = transaction_value_cents(tx);
        TransactionType    type  = transaction_type_get(tx);
        int32_t            dkey  = time_to_date_key(rec);

        bool to_owned;
        bool from_owned;
        if (arg->has_scope_account) {
            to_owned   = transaction_to_account_id(tx).value == arg->scope_account_id;
            from_owned = transaction_from_account_id(tx).value == arg->scope_account_id;
            if (!to_owned && !from_owned) continue;
        } else {
            to_owned   = id_in_set(arg->user_ids, arg->user_count,
                                   transaction_to_account_id(tx).value);
            from_owned = id_in_set(arg->user_ids, arg->user_count,
                                   transaction_from_account_id(tx).value);
        }

        int64_t net = (to_owned ? value : 0) - (from_owned ? value : 0);

        atomic_fetch_add_explicit(&s->transaction_count, 1, memory_order_relaxed);

        if (net > 0) {
            atomic_fetch_add_explicit(&s->total_incoming, net,
                                      memory_order_relaxed);
        } else if (net < 0) {
            atomic_fetch_add_explicit(&s->total_outgoing, -net,
                                      memory_order_relaxed);
        }

        atomic_fetch_add_explicit(&s->total_volume, net < 0 ? -net : net,
                                  memory_order_relaxed);

        if ((size_t)type < TX_SERVICE_TYPE_COUNT) {
            local_type.totals[type]  += value;
            local_type.present[type]  = true;
        }

        day_total_map_add(&local_day, dkey, net);
    }

    // 2. Merge thread-local maps under mutex.
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

// Sort transaction pointers oldest -> newest (id tie-break).
static int compare_tx_by_recorded_on(const void *a, const void *b) {
    const Transaction *ta = *(const Transaction *const *)a;
    const Transaction *tb = *(const Transaction *const *)b;
    time_t ra = transaction_recorded_on(ta);
    time_t rb = transaction_recorded_on(tb);
    if (ra != rb) return (ra > rb) - (ra < rb);
    long ma = transaction_recorded_on_micros(ta);
    long mb = transaction_recorded_on_micros(tb);
    if (ma != mb) return (ma > mb) - (ma < mb);
    int64_t ia = transaction_id(ta).value;
    int64_t ib = transaction_id(tb).value;
    return (ia > ib) - (ia < ib);
}

static int compare_entry_asc(const void *a, const void *b) {
    const AccountStatementEntry *ea = (const AccountStatementEntry *)a;
    const AccountStatementEntry *eb = (const AccountStatementEntry *)b;
    if (ea->recorded_on != eb->recorded_on)
        return (ea->recorded_on > eb->recorded_on) - (ea->recorded_on < eb->recorded_on);
    if (ea->recorded_on_micros != eb->recorded_on_micros)
        return (ea->recorded_on_micros > eb->recorded_on_micros)
             - (ea->recorded_on_micros < eb->recorded_on_micros);
    int64_t ia = ea->transaction_id.value;
    int64_t ib = eb->transaction_id.value;
    return (ia > ib) - (ia < ib);
}

static int compare_entry_desc(const void *a, const void *b) {
    return compare_entry_asc(b, a);
}

// Apply balance changes per transaction type, then persist accounts.
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

    // 1. Reject non-positive value before domain call.
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

    // 2. Open a DB transaction so balance update(s) + tx insert commit together.
    RepoError rerr;
    if (!svc->tx_repo.vtable->begin(svc->tx_repo.ctx, &rerr)) {
        transaction_free(tx);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    // 3. Update account balances (reject overdraft before insert).
    if (!record_transaction_apply_balances(svc, cmd, err)) {
        svc->tx_repo.vtable->rollback(svc->tx_repo.ctx);
        transaction_free(tx);
        return false;
    }

    // 4. Persist the transaction row.
    TransactionId new_id;
    bool ok = svc->tx_repo.vtable->insert(
        svc->tx_repo.ctx, tx, &new_id, &rerr);

    transaction_free(tx);

    if (!ok) {
        svc->tx_repo.vtable->rollback(svc->tx_repo.ctx);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    // 5. Commit; balances and the new row become visible together.
    if (!svc->tx_repo.vtable->commit(svc->tx_repo.ctx, &rerr)) {
        svc->tx_repo.vtable->rollback(svc->tx_repo.ctx);
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

void account_statement_result_free(AccountStatementResult *result) {
    if (!result) return;
    free(result->items);
    result->items = NULL;
    result->item_count = 0;
}

bool transaction_service_compute_account_statement(
    TransactionService *svc,
    const AccountStatementQuery *query,
    AccountStatementResult *out,
    ServiceError *err)
{
    if (!svc || !query || !out) {
        if (err) *err = service_error_validation(
            "TransactionService::compute_account_statement: invalid arguments");
        return false;
    }
    memset(out, 0, sizeof *out);

    AccountId account_id = query->account_id;

    // 1. Load current balance (anchor for running balance).
    Account  *account = NULL;
    RepoError rerr;
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, account_id, &account, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    int64_t current_balance = account_balance_cents(account);
    account_free(account);

    // 2. Load full history; filter/paginate after balance computation.
    Transaction **txs   = NULL;
    size_t        count = 0;
    if (!svc->tx_repo.vtable->list_for_account(
            svc->tx_repo.ctx, account_id, INT64_MAX, 0, &txs, &count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    // 3. Sort oldest -> newest.
    if (count > 1) {
        qsort(txs, count, sizeof(Transaction *), compare_tx_by_recorded_on);
    }

    // 4. Reconstruct opening balance from signed deltas.
    int64_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        int64_t val = transaction_value_cents(txs[i]);
        total += (transaction_to_account_id(txs[i]).value == account_id.value)
                 ? val : -val;
    }
    int64_t running = current_balance - total;

    // 5. Forward pass: compute balance-after per entry.
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
        entries[i].recorded_on_micros  = transaction_recorded_on_micros(tx);
        entries[i].transaction_type    = transaction_type_get(tx);
        entries[i].value_cents         = delta;
        entries[i].balance_after_cents = running;

        const char *desc = transaction_description(tx);
        if (desc) {
            strncpy(entries[i].description, desc,
                    sizeof(entries[i].description) - 1);
            entries[i].description[sizeof(entries[i].description) - 1] = '\0';
        }
    }

    free_tx_array(txs, count);

    // 6. Filter by date range (in-place).
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

    // 7. Optional transaction-type filter.
    if (query->has_tx_type) {
        size_t write = 0;
        for (size_t i = 0; i < count; ++i) {
            if (entries[i].transaction_type == query->tx_type) {
                entries[write++] = entries[i];
            }
        }
        count = write;
    }

    out->total_count = (int64_t)count;

    if (count > 0) {
        AccountStatementEntry *asc = (AccountStatementEntry *)malloc(
            count * sizeof(AccountStatementEntry));
        if (!asc) {
            free(entries);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
        memcpy(asc, entries, count * sizeof(AccountStatementEntry));
        qsort(asc, count, sizeof(AccountStatementEntry), compare_entry_asc);
        out->has_opening = true;
        out->opening_balance_cents = asc[0].balance_after_cents - asc[0].value_cents;
        out->has_closing = true;
        out->closing_balance_cents = asc[count - 1].balance_after_cents;
        free(asc);
    }

    // 8. Sort for display, then paginate.
    if (count > 1) {
        qsort(entries, count, sizeof(AccountStatementEntry),
              query->sort_newest_first ? compare_entry_desc : compare_entry_asc);
    }

    size_t off = (query->offset < 0) ? 0 : (size_t)query->offset;
    size_t lim = (query->limit  < 0) ? 0 : (size_t)query->limit;

    size_t start = (off < count) ? off : count;
    size_t n     = count - start;
    if (n > lim) n = lim;

    if (n == 0) {
        free(entries);
        if (err) *err = service_error_ok();
        return true;
    }

    out->items = (AccountStatementEntry *)malloc(n * sizeof(AccountStatementEntry));
    if (!out->items) {
        free(entries);
        if (err) *err = service_error_internal("out of memory");
        return false;
    }
    memcpy(out->items, entries + start, n * sizeof(AccountStatementEntry));
    out->item_count = n;
    free(entries);

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

    // 1. Load all user accounts.
    Account **accounts      = NULL;
    size_t    account_count = 0;
    RepoError rerr;

    if (!svc->account_repo.vtable->list_for_user(
            svc->account_repo.ctx, user_id,
            &accounts, &account_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

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

    // 2. Gather transactions from every account.
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
        free(chunk);
    }

    free_account_array(accounts, account_count);

    // 3. Dedupe by transaction id (sort + scan).
    all_count = dedupe_transactions_by_id(all_txs, all_count);

    // 4. Parallel aggregation across worker threads.
    long   nproc       = get_processor_count();
    size_t num_threads = (nproc > 0) ? (size_t)nproc : 1;
    if (all_count > 0 && num_threads > all_count) num_threads = all_count;
    if (num_threads == 0) num_threads = 1;

    size_t chunk_size = all_count / num_threads;
    if (chunk_size == 0) chunk_size = 1;

    StatsShared shared;
    atomic_init(&shared.total_incoming, 0);
    atomic_init(&shared.total_outgoing, 0);
    atomic_init(&shared.total_volume,   0);
    atomic_init(&shared.transaction_count, 0);

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

    // 5. Spawn workers and join.
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
        args[t].has_scope_account = false;
        args[t].scope_account_id  = 0;
        args[t].has_tx_type       = false;
        args[t].filter_type       = TRANSACTION_TYPE_DEPOSIT;

        if (pthread_create(&threads[t], NULL, stats_worker_fn, &args[t]) != 0) {
            for (size_t j = 0; j < t; ++j) pthread_join(threads[j], NULL);
            free(args);
            free(threads);
            free_tx_array(all_txs, all_count);
            free(user_ids);
            if (err) *err = service_error_internal("failed to spawn statistics worker thread");
            return false;
        }
        offset += slice;
    }

    for (size_t t = 0; t < num_threads; ++t) {
        pthread_join(threads[t], NULL);
    }

    free(args);
    free(threads);
    free_tx_array(all_txs, all_count);
    free(user_ids);

    // 6. Collect aggregated results.
    out->total_incoming_cents =
        atomic_load_explicit(&shared.total_incoming, memory_order_relaxed);
    out->total_outgoing_cents =
        atomic_load_explicit(&shared.total_outgoing, memory_order_relaxed);
    out->total_volume_cents   =
        atomic_load_explicit(&shared.total_volume,   memory_order_relaxed);

    out->per_type = shared.per_type;

    if (shared.per_day.count > 1) {
        qsort(shared.per_day.entries,
              shared.per_day.count,
              sizeof(DayTotal),
              compare_day_totals);
    }

    out->per_day       = shared.per_day.entries;
    out->per_day_count = shared.per_day.count;

    pthread_mutex_destroy(&shared.per_type_mutex);
    pthread_mutex_destroy(&shared.per_day_mutex);

    if (err) *err = service_error_ok();
    return true;
}

// Alphabetical order for payment categories so the JSON array is deterministic
// and matches the Rust backend's BTreeMap iteration order.
static int compare_payment_cat(const void *a, const void *b) {
    const PaymentCategoryTotal *ca = (const PaymentCategoryTotal *)a;
    const PaymentCategoryTotal *cb = (const PaymentCategoryTotal *)b;
    return strcmp(ca->category, cb->category);
}

static void parse_payment_category(const char *description, char *out, size_t out_sz) {
    snprintf(out, out_sz, "Other");
    if (!description) return;
    const char *p = description;
    while (*p) {
        while (*p == ' ' || *p == '|') ++p;
        if (strncmp(p, "category:", 9) == 0) {
            p += 9;
            while (*p == ' ') ++p;
            strncpy(out, p, out_sz - 1);
            out[out_sz - 1] = '\0';
            char *bar = strchr(out, '|');
            if (bar) *bar = '\0';
            for (char *c = out + strlen(out) - 1; c >= out && *c == ' '; --c) *c = '\0';
            if (out[0] != '\0') return;
        }
        const char *next = strchr(p, '|');
        p = next ? next + 1 : p + strlen(p);
    }
}

static bool tx_passes_filters(
    const Transaction *tx,
    time_t rec,
    bool has_from, time_t from,
    bool has_to, time_t to,
    const StatsFilterOpts *filters,
    const int64_t *user_ids,
    size_t user_count)
{
    if (has_from && rec < from) return false;
    if (has_to && rec > to) return false;
    if (filters && filters->has_tx_type && transaction_type_get(tx) != filters->tx_type) {
        return false;
    }
    if (filters && filters->has_scope_account) {
        bool to_owned   = transaction_to_account_id(tx).value == filters->scope_account_id;
        bool from_owned = transaction_from_account_id(tx).value == filters->scope_account_id;
        if (!to_owned && !from_owned) return false;
    }
    (void)user_ids;
    (void)user_count;
    return true;
}

bool transaction_service_compute_user_statistics_extended(
    TransactionService *svc,
    UserId user_id,
    int64_t per_account_limit,
    bool has_from,
    time_t from,
    bool has_to,
    time_t to,
    const StatsFilterOpts *filters,
    UserTransactionStatistics *out,
    int64_t *out_tx_count,
    PaymentCategoryTotal **out_payment_cats,
    size_t *out_payment_cat_count,
    ServiceError *err)
{
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "compute_user_statistics_extended: invalid arguments");
        return false;
    }

    memset(out, 0, sizeof *out);

    Account **accounts      = NULL;
    size_t    account_count = 0;
    RepoError rerr;

    if (!svc->account_repo.vtable->list_for_user(
            svc->account_repo.ctx, user_id, &accounts, &account_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    int64_t *user_ids = NULL;
    if (account_count > 0) {
        user_ids = (int64_t *)calloc(account_count, sizeof(int64_t));
        if (!user_ids) {
            free_account_array(accounts, account_count);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
        for (size_t a = 0; a < account_count; ++a) {
            user_ids[a] = account_id(accounts[a]).value;
        }
    }

    Transaction **all_txs = NULL;
    size_t all_count = 0, all_cap = 0;

    for (size_t a = 0; a < account_count; ++a) {
        AccountId aid = account_id(accounts[a]);
        Transaction **chunk = NULL;
        size_t chunk_count = 0;
        if (!svc->tx_repo.vtable->list_for_account(
                svc->tx_repo.ctx, aid, per_account_limit, 0, &chunk, &chunk_count, &rerr)) {
            free(user_ids);
            free_account_array(accounts, account_count);
            free_tx_array(all_txs, all_count);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (all_count + chunk_count > all_cap) {
            size_t new_cap = (all_cap + chunk_count) * 2;
            if (new_cap < 16) new_cap = 16;
            Transaction **buf = (Transaction **)realloc(all_txs, new_cap * sizeof(Transaction *));
            if (!buf) {
                free(chunk);
                free(user_ids);
                free_account_array(accounts, account_count);
                free_tx_array(all_txs, all_count);
                if (err) *err = service_error_internal("out of memory");
                return false;
            }
            all_txs = buf;
            all_cap = new_cap;
        }
        memcpy(all_txs + all_count, chunk, chunk_count * sizeof(Transaction *));
        all_count += chunk_count;
        free(chunk);
    }
    free_account_array(accounts, account_count);

    all_count = dedupe_transactions_by_id(all_txs, all_count);

    StatsShared shared;
    atomic_init(&shared.total_incoming, 0);
    atomic_init(&shared.total_outgoing, 0);
    atomic_init(&shared.total_volume, 0);
    atomic_init(&shared.transaction_count, 0);
    pthread_mutex_init(&shared.per_type_mutex, NULL);
    pthread_mutex_init(&shared.per_day_mutex, NULL);
    memset(&shared.per_type, 0, sizeof shared.per_type);
    day_total_map_init(&shared.per_day);

    long nproc = get_processor_count();
    size_t num_threads = (nproc > 0) ? (size_t)nproc : 1;
    if (all_count > 0 && num_threads > all_count) num_threads = all_count;
    if (num_threads == 0) num_threads = 1;
    size_t chunk_size = all_count / num_threads;
    if (chunk_size == 0) chunk_size = 1;

    StatsWorkerArgs *args = (StatsWorkerArgs *)calloc(num_threads, sizeof *args);
    pthread_t *threads = (pthread_t *)calloc(num_threads, sizeof *threads);
    if (!args || !threads) {
        free(args); free(threads);
        pthread_mutex_destroy(&shared.per_type_mutex);
        pthread_mutex_destroy(&shared.per_day_mutex);
        day_total_map_free(&shared.per_day);
        free_tx_array(all_txs, all_count);
        free(user_ids);
        if (err) *err = service_error_internal("out of memory");
        return false;
    }

    size_t offset = 0;
    for (size_t t = 0; t < num_threads; ++t) {
        size_t slice = (t == num_threads - 1) ? (all_count - offset) : chunk_size;
        args[t].slice = all_txs + offset;
        args[t].count = slice;
        args[t].shared = &shared;
        args[t].user_ids = user_ids;
        args[t].user_count = account_count;
        args[t].has_from = has_from;
        args[t].from = from;
        args[t].has_to = has_to;
        args[t].to = to;
        args[t].has_scope_account = filters && filters->has_scope_account;
        args[t].scope_account_id  = filters ? filters->scope_account_id : 0;
        args[t].has_tx_type       = filters && filters->has_tx_type;
        args[t].filter_type       = filters ? filters->tx_type : TRANSACTION_TYPE_DEPOSIT;
        if (pthread_create(&threads[t], NULL, stats_worker_fn, &args[t]) != 0) {
            for (size_t j = 0; j < t; ++j) pthread_join(threads[j], NULL);
            free(args);
            free(threads);
            free_tx_array(all_txs, all_count);
            free(user_ids);
            if (err) *err = service_error_internal("failed to spawn statistics worker thread");
            return false;
        }
        offset += slice;
    }
    for (size_t t = 0; t < num_threads; ++t) pthread_join(threads[t], NULL);
    free(args);
    free(threads);

    out->total_incoming_cents = atomic_load_explicit(&shared.total_incoming, memory_order_relaxed);
    out->total_outgoing_cents = atomic_load_explicit(&shared.total_outgoing, memory_order_relaxed);
    out->total_volume_cents   = atomic_load_explicit(&shared.total_volume, memory_order_relaxed);
    out->per_type = shared.per_type;
    if (shared.per_day.count > 1) {
        qsort(shared.per_day.entries, shared.per_day.count, sizeof(DayTotal), compare_day_totals);
    }
    out->per_day = shared.per_day.entries;
    out->per_day_count = shared.per_day.count;

    if (out_tx_count) {
        *out_tx_count = atomic_load_explicit(&shared.transaction_count, memory_order_relaxed);
    }

    if (out_payment_cats && out_payment_cat_count) {
        PaymentCategoryTotal *cats = NULL;
        size_t cat_count = 0, cat_cap = 0;
        for (size_t i = 0; i < all_count; ++i) {
            const Transaction *tx = all_txs[i];
            time_t rec = transaction_recorded_on(tx);
            if (!tx_passes_filters(tx, rec, has_from, from, has_to, to, filters, user_ids, account_count)) {
                continue;
            }
            if (transaction_type_get(tx) != TRANSACTION_TYPE_PAYMENT) continue;

            char category[64];
            parse_payment_category(transaction_description(tx), category, sizeof category);
            int64_t value = transaction_value_cents(tx);

            size_t found = cat_count;
            for (size_t c = 0; c < cat_count; ++c) {
                if (strcmp(cats[c].category, category) == 0) {
                    cats[c].total_cents += value;
                    found = c;
                    break;
                }
            }
            if (found < cat_count) continue;

            if (cat_count >= cat_cap) {
                size_t new_cap = cat_cap ? cat_cap * 2 : 8;
                PaymentCategoryTotal *buf = (PaymentCategoryTotal *)realloc(
                    cats, new_cap * sizeof(PaymentCategoryTotal));
                if (!buf) break;
                cats = buf;
                cat_cap = new_cap;
            }
            strncpy(cats[cat_count].category, category, sizeof cats[cat_count].category - 1);
            cats[cat_count].category[sizeof cats[cat_count].category - 1] = '\0';
            cats[cat_count].total_cents = value;
            cat_count++;
        }
        if (cats && cat_count > 1) {
            qsort(cats, cat_count, sizeof *cats, compare_payment_cat);
        }
        *out_payment_cats = cats;
        *out_payment_cat_count = cat_count;
    }

    pthread_mutex_destroy(&shared.per_type_mutex);
    pthread_mutex_destroy(&shared.per_day_mutex);
    free_tx_array(all_txs, all_count);
    free(user_ids);

    if (err) *err = service_error_ok();
    return true;
}

void user_transaction_statistics_free(UserTransactionStatistics *stats) {
    if (!stats) return;
    free(stats->per_day);
    stats->per_day       = NULL;
    stats->per_day_count = 0;
}

// Verify account exists and belongs to user (NotFound on mismatch).
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

// Civil UTC -> time_t (portable; matches Rust UTC bounds).
static time_t utc_civil_to_time(int year, int month, int day,
                                int hour, int min, int sec) {
    int y = year;
    y -= (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = (long long)era * 146097 + (long long)doe - 719468;
    return (time_t)(days * 86400 + hour * 3600 + min * 60 + sec);
}

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
    int64_t limit, int64_t offset,
    Transaction ***out_txs, size_t *out_count, ServiceError *err)
{
    if (!svc || !out_txs || !out_count) {
        if (err) *err = service_error_validation("list_recent_for_user: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;

    if (limit <= 0) limit = 10;
    if (limit < 1)  limit = 1;
    if (offset < 0) offset = 0;

    return transaction_service_list_for_account(
        svc, account_id, limit, offset, out_txs, out_count, err);
}

bool transaction_service_compute_account_statement_for_user_from_strings(
    TransactionService *svc, UserId user_id, AccountId account_id,
    const char *from_opt, const char *to_opt,
    const char *transaction_type_opt, const char *sort_opt,
    int64_t limit, int64_t offset,
    AccountStatementResult *out, ServiceError *err)
{
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "compute_account_statement_for_user_from_strings: invalid arguments");
        return false;
    }
    if (!ensure_account_owned_by(svc, user_id, account_id, err)) return false;

    AccountStatementQuery query;
    memset(&query, 0, sizeof query);
    query.account_id = account_id;
    query.limit      = (limit <= 0)  ? 100 : limit;
    query.offset     = (offset < 0)  ? 0   : offset;
    query.sort_newest_first = (sort_opt && strcmp(sort_opt, "newest") == 0);

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

    if (transaction_type_opt && transaction_type_opt[0]
        && strcmp(transaction_type_opt, "All") != 0) {
        TransactionType tt;
        DomainError derr;
        if (!transaction_type_from_str(transaction_type_opt, &tt, &derr)) {
            if (err) *err = service_error_validation("invalid transaction_type");
            return false;
        }
        query.has_tx_type = true;
        query.tx_type = tt;
    }

    return transaction_service_compute_account_statement(
        svc, &query, out, err);
}