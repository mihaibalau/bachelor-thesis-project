#ifndef HTTP_STATE_H
#define HTTP_STATE_H

#include <stddef.h>
#include "service/include/user_service.h"
#include "service/include/account_service.h"
#include "service/include/transaction_service.h"
#include "service/include/affiliate_service.h"
#include "service/include/dashboard_service.h"

#define JWT_SECRET_MAX 256

typedef struct AppState {
    UserService      *user_svc;
    AccountService   *account_svc;
    TransactionService *tx_svc;
    AffiliateService *affiliate_svc;
    DashboardService *dashboard_svc;
    char              jwt_secret[JWT_SECRET_MAX];
} AppState;

void app_state_init(
    AppState *state,
    UserService *user_svc,
    AccountService *account_svc,
    TransactionService *tx_svc,
    AffiliateService *affiliate_svc,
    DashboardService *dashboard_svc,
    const char *jwt_secret
);

#endif