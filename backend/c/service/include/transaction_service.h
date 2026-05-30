#ifndef C_TRANSACTION_SERVICE_H
#define C_TRANSACTION_SERVICE_H

/*
 * TransactionService: transaction recording, listing, and analytics.
 *
 * This is the most feature-rich service in the project — it exercises
 * every chapter of the thesis table of contents:
 *
 * §3.1  Encapsulation         : TransactionService + both repos are opaque.
 * §3.2  Methods               : transaction_service_* function namespace.
 * §3.3  Polymorphism          : explicit vtables for both repo abstractions.
 * §3.4  Composition           : TransactionService has tx_repo + account_repo.
 * §3.5  Layered arch.         : service never touches Db* or PGconn*.
 * §4.1  Type-Driven Invariants: value > 0 + Transaction domain constructor.
 * §4.2  Error Propagation     : DomainError/RepoError -> ServiceError.
 * §4.3.1 Threads              : pthread_create/join in compute_user_statistics,
 *                               mirroring Rust's std::thread::scope.
 * §4.3.2 Mutexes              : pthread_mutex_t guards shared per-type and
 *                               per-day maps, mirroring Arc<Mutex<...>>.
 * §4.3.3 Async / Atomics      : C11 _Atomic int64_t counters mirror Rust's
 *                               AtomicI64 with Ordering::Relaxed.
 *                               (C has no async/await; all I/O is synchronous —
 *                               this contrast is itself a thesis talking point.)
 * §4.4  Testing               : vtables allow in-memory fake repos.
 */

#include <stdbool.h>
#include <stdint.h>

#include "ids.h"
#include "transaction.h"
#include "transaction_type.h"
#include "account.h"
#include "repo_error.h"
#include "service_error.h"
#include "transaction_repo.h"
#include "account_repo.h"

/*
 * Repository abstractions (ports) with explicit vtables.
 *
 * TransactionRepositoryVTable mirrors the Rust trait:
 *
 *   pub trait TransactionRepository: Send + Sync {
 *       async fn get_by_id(...)        -> Result<Transaction, RepoError>;
 *       async fn insert(...)           -> Result<TransactionId, RepoError>;
 *       async fn list_for_account(...) -> Result<Vec<Transaction>, RepoError>;
 *   }
 *
 */
typedef struct TransactionRepositoryVTable {

    bool (*get_by_id)(
        void *ctx,
        TransactionId id,
        Transaction **out,
        RepoError *err);

    bool (*insert)(
        void *ctx,
        const Transaction *tx,
        TransactionId *out_id,
        RepoError *err);

    bool (*list_for_account)(
        void *ctx,
        AccountId account_id,
        int64_t limit,
        int64_t offset,
        Transaction ***out_txs,
        size_t *out_count,
        RepoError *err);

} TransactionRepositoryVTable;

typedef struct TransactionRepository {
    const TransactionRepositoryVTable *vtable;
    void *ctx;
} TransactionRepository;

/*
 * AccountRepository abstraction (re-used from the account service).
 *
 * TransactionService needs account_repo only for list_for_user inside
 * compute_user_statistics.  We define a minimal vtable here rather than
 * dragging in the full account_service.h.
 */
typedef struct TxAccountRepositoryVTable {

    bool (*list_for_user)(
        void *ctx,
        UserId user_id,
        Account ***out_accounts,
        size_t *out_count,
        RepoError *err);

    /* Needed for ownership checks and same-currency rules on send/transfer. */
    bool (*get_by_id)(
        void *ctx,
        AccountId account_id,
        Account **out,
        RepoError *err);

    /* Needed to persist balance changes when recording a transaction. */
    bool (*update)(
        void *ctx,
        const Account *account,
        RepoError *err);

} TxAccountRepositoryVTable;

typedef struct TxAccountRepository {
    const TxAccountRepositoryVTable *vtable;
    void *ctx;
} TxAccountRepository;

/*
 * Concrete adapters from DB-layer repositories to these abstractions.
 *
 * These are your "ports and adapters" in C.
 */

TransactionRepository tx_repository_from_repo(TransactionRepo *repo);
TxAccountRepository   tx_account_repository_from_repo(AccountRepo *repo);

/* ---- DTOs / read models ---------------------------------------------- */

typedef struct RecordTransactionCommand {
    AccountId       from_account_id;
    AccountId       to_account_id;
    TransactionType transaction_type;
    int64_t         value_cents;
    const char     *description;
} RecordTransactionCommand;

/*
 * Option<DateTime<Utc>> is represented as (bool has_*, time_t *) pairs.
 * This is the idiomatic C encoding of Rust Option — the absence of a
 * discriminated union forces us to be explicit.
 */
typedef struct AccountStatementQuery {
    AccountId account_id;
    bool      has_from;
    time_t    from;      /* inclusive lower bound; ignored if !has_from */
    bool      has_to;
    time_t    to;        /* inclusive upper bound; ignored if !has_to   */
    int64_t   limit;
    int64_t   offset;
} AccountStatementQuery;

typedef struct AccountStatementEntry {
    TransactionId   transaction_id;
    time_t          recorded_on;
    char            description[256];
    TransactionType transaction_type;
    int64_t         value_cents;
    int64_t         balance_after_cents;   /* running balance after this tx */
} AccountStatementEntry;

/*
 * Rust uses BTreeMap<NaiveDate, i64>.  In C we maintain a sorted
 * dynamic array of (date_key, total) pairs.
 * date_key = YYYYMMDD as int32_t for cheap integer comparison.
 *
 * This is a deliberate thesis talking-point: Rust's BTreeMap<K,V>
 * gives us a sorted associative container "for free"; in C we must
 * build one from scratch.
 */
typedef struct DayTotal {
    int32_t date_key;   /* YYYYMMDD */
    int64_t total;
} DayTotal;

/*
 * PerTypeTotals: per-transaction-type volume.
 *
 * Mirrors Rust's HashMap<TransactionType, i64>.  Indexed directly by
 * the TransactionType enum value — allocation-free, cache-friendly.
 */
#define TX_SERVICE_TYPE_COUNT 8   /* must be > max TransactionType enum value */

typedef struct PerTypeTotals {
    int64_t totals[TX_SERVICE_TYPE_COUNT];
    bool    present[TX_SERVICE_TYPE_COUNT];
} PerTypeTotals;

/*
 * Designed to show off multi-threaded aggregation using atomics and
 * mutexes (§4.3.1, §4.3.2, §4.3.3).
 */
typedef struct UserTransactionStatistics {
    int64_t      total_incoming_cents;
    int64_t      total_outgoing_cents;
    int64_t      total_volume_cents;
    PerTypeTotals per_type;
    DayTotal     *per_day;          /* caller must free() */
    size_t        per_day_count;
} UserTransactionStatistics;

/* ---- TransactionService "object" -------------------------------------- */

typedef struct TransactionService TransactionService;

/* Constructor / Destructor */
TransactionService *transaction_service_new(TransactionRepository tx_repo,
                                            TxAccountRepository   account_repo);
void                transaction_service_free(TransactionService *svc);

/* Methods */

/*
 * record_transaction: validate and persist a new transaction.
 *
 * §4.1: value_cents > 0 is checked before calling the domain constructor.
 *       transaction_create then enforces all remaining invariants
 *       (from != to, value >= 0, non-empty description).
 *
 * Mirrors:
 *   pub async fn record_transaction(&self, cmd: RecordTransactionCommand)
 *       -> ServiceResult<TransactionId>
 */
bool transaction_service_record_transaction(
    TransactionService *svc,
    const RecordTransactionCommand *cmd,
    TransactionId *out_id,
    ServiceError *err
);

/*
 * list_for_account: raw paginated listing for one account.
 *
 * Mirrors:
 *   pub async fn list_for_account(&self, account_id, limit, offset)
 *       -> ServiceResult<Vec<Transaction>>
 *
 * Caller owns the returned array; each Transaction* must be freed with
 * transaction_free(), then the array itself with free().
 */
bool transaction_service_list_for_account(
    TransactionService *svc,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    ServiceError *err
);

/*
 * compute_account_statement: build a statement with running balances
 * and optional time-range filter.
 *
 * In Rust this uses `tokio::task::spawn_blocking` to protect the async
 * executor from the CPU-heavy sort + accumulation.  In C all I/O is
 * already blocking, so the computation runs directly on the calling
 * thread — a key architectural difference discussed in §4.3.3.
 *
 * Steps (matching Rust exactly):
 *   1. Load transactions via list_for_account.
 *   2. Filter by [from, to] time range.
 *   3. Sort oldest -> newest (qsort, compare by recorded_on).
 *   4. Walk sorted list, accumulate running balance.
 *   5. Return array of AccountStatementEntry.
 *
 * Caller owns *out_entries (must free()).
 */
bool transaction_service_compute_account_statement(
    TransactionService *svc,
    const AccountStatementQuery *query,
    AccountStatementEntry **out_entries,
    size_t *out_count,
    ServiceError *err
);

/*
 * compute_user_statistics: multi-threaded aggregation across all
 * accounts of a user.
 *
 * §4.3.1 Threads  : spawns sysconf(_SC_NPROCESSORS_ONLN) pthreads, each
 *                   processing a contiguous chunk of the transaction array.
 *                   Mirrors Rust's thread::scope { for chunk in chunks { scope.spawn(...) } }.
 * §4.3.2 Mutexes  : per-type and per-day maps are protected by a
 *                   pthread_mutex_t during the merge phase.
 *                   Mirrors Arc<Mutex<HashMap<...>>>.
 * §4.3.3 Atomics  : C11 _Atomic int64_t for incoming/outgoing/volume totals.
 *                   Mirrors Rust's AtomicI64::fetch_add(v, Ordering::Relaxed).
 *
 * Caller must call user_transaction_statistics_free() when done.
 */
bool transaction_service_compute_user_statistics(
    TransactionService *svc,
    UserId user_id,
    int64_t per_account_limit,
    bool has_from,                  /* restrict to recorded_on >= from */
    time_t from,
    bool has_to,                    /* restrict to recorded_on <= to   */
    time_t to,
    UserTransactionStatistics *out,
    ServiceError *err
);

/* Free heap memory inside a UserTransactionStatistics (per_day array). */
void user_transaction_statistics_free(UserTransactionStatistics *stats);

/* ── User-facing use-cases (thin wrappers used by the HTTP layer) ─────────── */
/*
 * Each of these mirrors the matching Rust `*_for_user` method: it first
 * verifies the relevant account(s) belong to `user_id`, then applies the
 * deposit/withdrawal/send/transfer/payment business rules (amount sign, unit
 * → cents conversion, same-currency checks, description formatting) before
 * recording the transaction. All of this is business logic and lives here,
 * never in the server layer.
 *
 * `amount` parameters named *_units are whole currency units (no cents) and
 * are multiplied by 100, exactly like the Rust DepositRequest/PaymentRequest.
 */

bool transaction_service_record_deposit_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    int64_t amount_units,
    TransactionId *out_id,
    ServiceError *err
);

bool transaction_service_record_withdrawal_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    int64_t amount_units,
    TransactionId *out_id,
    ServiceError *err
);

bool transaction_service_record_send_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId from_account_id,
    AccountId recipient_account_id,
    int64_t value_cents,
    const char *message,
    TransactionId *out_id,
    ServiceError *err
);

bool transaction_service_record_transfer_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId from_account_id,
    AccountId to_account_id,
    int64_t value_cents,
    TransactionId *out_id,
    ServiceError *err
);

bool transaction_service_record_payment_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId from_account_id,
    int64_t amount_units,
    const char *category,
    const char *merchant_name,
    const char *note_opt,           /* NULL if absent */
    TransactionId *out_id,
    ServiceError *err
);

/*
 * list_recent_for_user: ownership-checked recent listing.
 * `limit` defaults to 10 (when <= 0) and is floored at 1, mirroring Rust.
 * Caller owns *out_txs (free each Transaction* then the array).
 */
bool transaction_service_list_recent_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    int64_t limit,                  /* <= 0 means "unset" → default 10 */
    Transaction ***out_txs,
    size_t *out_count,
    ServiceError *err
);

/*
 * compute_account_statement_for_user_from_strings: ownership-checked statement
 * with optional "YYYY-MM-DD" date bounds parsed here in the service.
 * `from`/`to` may be NULL. `limit`/`offset` <= 0 fall back to 100 / 0.
 * Caller owns *out_entries (free()).
 */
bool transaction_service_compute_account_statement_for_user_from_strings(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    const char *from_opt,           /* NULL or "YYYY-MM-DD" */
    const char *to_opt,             /* NULL or "YYYY-MM-DD" */
    int64_t limit,                  /* <= 0 → 100 */
    int64_t offset,                 /* <  0 → 0   */
    AccountStatementEntry **out_entries,
    size_t *out_count,
    ServiceError *err
);

#endif //C_TRANSACTION_SERVICE_H