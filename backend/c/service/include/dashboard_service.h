#ifndef C_DASHBOARD_SERVICE_H
#define C_DASHBOARD_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ids.h"
#include "service_error.h"
#include "user_service.h"
#include "transaction_service.h"
#include "affiliate_service.h"

typedef struct DashboardActivityItem {
    int64_t id;
    char label[256];
    char description[256];
    char recorded_on[40];
    int64_t amount_cents;
    char category[32];
    bool is_income;
} DashboardActivityItem;

typedef struct DashboardDailySpendingPoint {
    char day_label[16];
    char date[11];
    int64_t cumulative_spending_cents;
} DashboardDailySpendingPoint;

typedef struct DashboardSpending {
    int64_t total_spent_cents;
    double change_percent_vs_last_month;
    DashboardDailySpendingPoint *daily_cumulative;
    size_t daily_count;
} DashboardSpending;

typedef struct DashboardData {
    int64_t total_balance_cents;
    double balance_change_percent;
    uint32_t active_accounts_count;
    uint64_t affiliates_count;
    int64_t transfers_total_cents;
    DashboardActivityItem *recent_activity;
    size_t recent_count;
    DashboardSpending spending;
} DashboardData;

typedef struct DashboardService {
    UserService *user_svc;
    TransactionService *tx_svc;
    AffiliateService *affiliate_svc;
} DashboardService;

DashboardService *dashboard_service_new(
    UserService *user_svc,
    TransactionService *tx_svc,
    AffiliateService *affiliate_svc
);

void dashboard_service_free(DashboardService *svc);

void dashboard_data_free(DashboardData *data);

bool dashboard_service_get(
    DashboardService *svc,
    UserId user_id,
    DashboardData *out,
    ServiceError *err
);

#endif
