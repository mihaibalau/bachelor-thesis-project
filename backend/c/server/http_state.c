#include "include/http_state.h"
#include <string.h>

/*
 * AppState = all services + JWT secret for one process.
 * libmicrohttpd passes this pointer into http_request_handler as void* cls.
 */
void app_state_init(
    AppState *state,
    UserService *user_svc,
    AccountService *account_svc,
    TransactionService *tx_svc,
    AffiliateService *affiliate_svc,
    DashboardService *dashboard_svc,
    const char *jwt_secret
) {
    state->user_svc       = user_svc;
    state->account_svc    = account_svc;
    state->tx_svc         = tx_svc;
    state->affiliate_svc  = affiliate_svc;
    state->dashboard_svc  = dashboard_svc;
    if (!jwt_secret) jwt_secret = "";
    strncpy(state->jwt_secret, jwt_secret, JWT_SECRET_MAX - 1);
    state->jwt_secret[JWT_SECRET_MAX - 1] = '\0';
}
