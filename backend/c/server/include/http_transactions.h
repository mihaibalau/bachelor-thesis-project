#ifndef HTTP_TRANSACTIONS_H
#define HTTP_TRANSACTIONS_H

#include <microhttpd.h>
#include "http_state.h"

/*
 * Transactions HTTP routes
 *   POST /api/transactions/deposit         → record_deposit
 *   POST /api/transactions/withdrawal      → record_withdrawal
 *   POST /api/transactions/send            → record_send
 *   POST /api/transactions/transfer        → record_transfer
 *   POST /api/transactions/payment         → record_payment
 *   GET  /api/transactions/recent          → get_recent_transactions
 *   GET  /api/transactions/statement       → get_account_statement
 *   GET  /api/transactions/summary/monthly → get_user_monthly_summary
 */
enum MHD_Result http_transactions_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
);

#endif /* HTTP_TRANSACTIONS_H */
