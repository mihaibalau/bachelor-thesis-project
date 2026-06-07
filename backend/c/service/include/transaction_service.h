#ifndef C_TRANSACTION_SERVICE_H
#define C_TRANSACTION_SERVICE_H

// Transaction recording, listing, statement, and multi-threaded statistics.

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

// TransactionRepository port (vtable); mirrors Rust TransactionRepository trait.
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

// Minimal account port used by TransactionService (list, get, update balances).
typedef struct TxAccountRepositoryVTable {

    bool (*list_for_user)(
        void *ctx,
        UserId user_id,
        Account ***out_accounts,
        size_t *out_count,
        RepoError *err);

    bool (*get_by_id)(
        void *ctx,
        AccountId account_id,
        Account **out,
        RepoError *err);

    bool (*update)(
        void *ctx,
        const Account *account,
        RepoError *err);

} TxAccountRepositoryVTable;

typedef struct TxAccountRepository {
    const TxAccountRepositoryVTable *vtable;
    void *ctx;
} TxAccountRepository;

// Wire concrete DB repos into the vtable ports above.
TransactionRepository tx_repository_from_repo(TransactionRepo *repo);
TxAccountRepository   tx_account_repository_from_repo(AccountRepo *repo);

typedef struct RecordTransactionCommand {
    AccountId       from_account_id;
    AccountId       to_account_id;
    TransactionType transaction_type;
    int64_t         value_cents;
    const char     *description;
} RecordTransactionCommand;

// Optional date range: has_from/has_to gate the from/to timestamps.
typedef struct AccountStatementQuery {
    AccountId account_id;
    bool      has_from;
    time_t    from;
    bool      has_to;
    time_t    to;
    int64_t   limit;
    int64_t   offset;
} AccountStatementQuery;

typedef struct AccountStatementEntry {
    TransactionId   transaction_id;
    time_t          recorded_on;
    char            description[256];
    TransactionType transaction_type;
    int64_t         value_cents;
    int64_t         balance_after_cents;
} AccountStatementEntry;

// Daily aggregate; date_key is YYYYMMDD for sorting.
typedef struct DayTotal {
    int32_t date_key;
    int64_t total;
} DayTotal;

// Per-type volume totals indexed by TransactionType enum.
#define TX_SERVICE_TYPE_COUNT 8

typedef struct PerTypeTotals {
    int64_t totals[TX_SERVICE_TYPE_COUNT];
    bool    present[TX_SERVICE_TYPE_COUNT];
} PerTypeTotals;

typedef struct UserTransactionStatistics {
    int64_t      total_incoming_cents;
    int64_t      total_outgoing_cents;
    int64_t      total_volume_cents;
    PerTypeTotals per_type;
    DayTotal     *per_day;  // caller must free()
    size_t        per_day_count;
} UserTransactionStatistics;

typedef struct StatsFilterOpts {
    bool    has_scope_account;
    int64_t scope_account_id;
    bool    has_tx_type;
    TransactionType tx_type;
} StatsFilterOpts;

typedef struct PaymentCategoryTotal {
    char    category[64];
    int64_t total_cents;
} PaymentCategoryTotal;

typedef struct TransactionService TransactionService;

TransactionService *transaction_service_new(TransactionRepository tx_repo,
                                            TxAccountRepository   account_repo);
void                transaction_service_free(TransactionService *svc);

// Validate domain rules, persist tx, update account balances.
bool transaction_service_record_transaction(
    TransactionService *svc,
    const RecordTransactionCommand *cmd,
    TransactionId *out_id,
    ServiceError *err
);

// Paginated tx list for one account; caller frees txs then array.
bool transaction_service_list_for_account(
    TransactionService *svc,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    ServiceError *err
);

// Statement with running balance; optional date filter; caller frees entries.
bool transaction_service_compute_account_statement(
    TransactionService *svc,
    const AccountStatementQuery *query,
    AccountStatementEntry **out_entries,
    size_t *out_count,
    ServiceError *err
);

// Multi-threaded stats across all user accounts; call user_transaction_statistics_free after.
bool transaction_service_compute_user_statistics(
    TransactionService *svc,
    UserId user_id,
    int64_t per_account_limit,
    bool has_from,
    time_t from,
    bool has_to,
    time_t to,
    UserTransactionStatistics *out,
    ServiceError *err
);

// Statistics + filters, tx count, payment categories (Statistics page).
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
    ServiceError *err
);

void user_transaction_statistics_free(UserTransactionStatistics *stats);

// HTTP-facing use-cases: ownership check, then record_* (amount_units → cents).
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
    const char *note_opt,
    TransactionId *out_id,
    ServiceError *err
);

// Recent txs for one owned account; limit defaults to 10.
bool transaction_service_list_recent_for_user(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    int64_t limit,
    int64_t offset,
    Transaction ***out_txs,
    size_t *out_count,
    ServiceError *err
);

// Statement with optional YYYY-MM-DD bounds; caller frees entries.
bool transaction_service_compute_account_statement_for_user_from_strings(
    TransactionService *svc,
    UserId user_id,
    AccountId account_id,
    const char *from_opt,
    const char *to_opt,
    int64_t limit,
    int64_t offset,
    AccountStatementEntry **out_entries,
    size_t *out_count,
    ServiceError *err
);

#endif //C_TRANSACTION_SERVICE_H