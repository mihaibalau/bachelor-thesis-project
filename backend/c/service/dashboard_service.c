#include "include/dashboard_service.h"
#include "domain/include/transaction.h"
#include "domain/include/transaction_type.h"
#include "domain/include/account.h"
#include "domain/include/user.h"
#include "domain/include/ids.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/*
 * DashboardService orchestrates other services (no own DB port).
 * Holds pointers to UserService, TransactionService, AffiliateService only.
 */

// Format time_t + microseconds as RFC3339 UTC, matching chrono's to_rfc3339()
// "AutoSi" sub-second rule (see http_transactions.c).
static void rfc3339_utc(time_t t, long micros, char *buf, size_t buf_size) {
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char base[32];
    strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tmv);

    if (micros <= 0) {
        snprintf(buf, buf_size, "%s+00:00", base);
    } else if (micros % 1000 == 0) {
        snprintf(buf, buf_size, "%s.%03ld+00:00", base, micros / 1000);
    } else {
        snprintf(buf, buf_size, "%s.%06ld+00:00", base, micros);
    }
}

static time_t utc_civil_to_time_h(int year, int month, int day,
                                  int hour, int min, int sec) {
    // Proleptic Gregorian civil date → UTC epoch (no local TZ).
    int y = year;
    y -= (month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = (long long)era * 146097 + (long long)doe - 719468;
    return (time_t)(days * 86400 + hour * 3600 + min * 60 + sec);
}

static double percent_change(int64_t current, int64_t previous) {
    if (previous == 0) return current == 0 ? 0.0 : 100.0;
    return ((double)(current - previous) / (double)previous) * 100.0;
}

static void free_user_with_accounts(UserWithAccounts *dto) {
    if (!dto) return;
    user_free(dto->user);
    for (size_t i = 0; i < dto->account_count; ++i) account_free(dto->accounts[i]);
    free(dto->accounts);
    dto->user = NULL;
    dto->accounts = NULL;
    dto->account_count = 0;
}

static const char *weekday_short(int wday) {
    static const char *names[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    if (wday < 0 || wday > 6) return "Mon";
    return names[wday];
}

// Payment descriptions: "Payment | category: X | merchant: Y | note: Z" (pipe-separated).
static void parse_payment_fields(const char *description, char *merchant, size_t merchant_sz,
                                 char *category, size_t category_sz) {
    snprintf(merchant, merchant_sz, "Payment");
    snprintf(category, category_sz, "shopping");
    if (!description) return;

    const char *p = description;
    while (*p) {
        while (*p == ' ' || *p == '|') ++p;
        if (strncmp(p, "category:", 9) == 0) {
            p += 9;
            while (*p == ' ') ++p;
            strncpy(category, p, category_sz - 1);
            category[category_sz - 1] = '\0';
            char *bar = strchr(category, '|');
            if (bar) *bar = '\0';
            for (char *c = category + strlen(category) - 1; c >= category && *c == ' '; --c) *c = '\0';
        } else if (strncmp(p, "merchant:", 9) == 0) {
            p += 9;
            while (*p == ' ') ++p;
            strncpy(merchant, p, merchant_sz - 1);
            merchant[merchant_sz - 1] = '\0';
            char *bar = strchr(merchant, '|');
            if (bar) *bar = '\0';
            for (char *c = merchant + strlen(merchant) - 1; c >= merchant && *c == ' '; --c) *c = '\0';
        }
        const char *next = strchr(p, '|');
        p = next ? next + 1 : p + strlen(p);
    }
}

// Map tx type (+ payment category heuristics) to a UI icon bucket.
static const char *ui_category(TransactionType t, const char *payment_category, const char *description) {
    if (t == TRANSACTION_TYPE_WITHDRAWAL) return "atm";
    if (t == TRANSACTION_TYPE_SEND || t == TRANSACTION_TYPE_TRANSFER) return "transfer";
    if (t == TRANSACTION_TYPE_DEPOSIT) return "salary";
    if (t == TRANSACTION_TYPE_PAYMENT) {
        if (payment_category) {
            char lower[128];
            strncpy(lower, payment_category, sizeof lower - 1);
            lower[sizeof lower - 1] = '\0';
            for (char *c = lower; *c; ++c) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
            if (strstr(lower, "food") || strstr(lower, "dinner") || strstr(lower, "restaurant")) return "food";
        }
        if (description && strstr(description, "Glovo")) return "food";
        return "shopping";
    }
    return "shopping";
}

typedef struct {
    Transaction *tx;
    int64_t id;
} TxSortEntry;

static int tx_sort_desc(const void *a, const void *b) {
    const TxSortEntry *ta = (const TxSortEntry *)a;
    const TxSortEntry *tb = (const TxSortEntry *)b;
    time_t ra = transaction_recorded_on(ta->tx);
    time_t rb = transaction_recorded_on(tb->tx);
    if (ra > rb) return -1;
    if (ra < rb) return 1;
    if (ta->id > tb->id) return -1;
    if (ta->id < tb->id) return 1;
    return 0;
}

static void map_activity_item(
    const Transaction *tx,
    const AccountId *owned,
    size_t owned_count,
    DashboardActivityItem *out
) {
    AccountId from = transaction_from_account_id(tx);
    AccountId to   = transaction_to_account_id(tx);

    bool from_owned = false, to_owned = false;
    for (size_t i = 0; i < owned_count; ++i) {
        if (owned[i].value == from.value) from_owned = true;
        if (owned[i].value == to.value)   to_owned = true;
    }

    /* Signed amount from the user's perspective: + if money arrives at an owned
     * account from outside, − if it leaves an owned account. */
    int64_t amount_cents = 0;
    if (to_owned && !from_owned) amount_cents = transaction_value_cents(tx);
    else if (from_owned) amount_cents = -transaction_value_cents(tx);

    TransactionType t = transaction_type_get(tx);
    const char *description = transaction_description(tx);

    char label[256] = "Transaction";
    char detail[256] = "";
    char pay_merchant[128], pay_category[128];
    parse_payment_fields(description, pay_merchant, sizeof pay_merchant, pay_category, sizeof pay_category);

    if (t == TRANSACTION_TYPE_PAYMENT) {
        snprintf(label, sizeof label, "%s", pay_merchant);
        const char *note = strstr(description, "note:");
        if (note) {
            note += 5;
            while (*note == ' ') ++note;
            strncpy(detail, note, sizeof detail - 1);
            detail[sizeof detail - 1] = '\0';
            char *bar = strchr(detail, '|');
            if (bar) *bar = '\0';
        } else {
            snprintf(detail, sizeof detail, "%s", pay_category);
        }
    } else if (t == TRANSACTION_TYPE_WITHDRAWAL) {
        snprintf(label, sizeof label, "ATM Withdrawal");
        snprintf(detail, sizeof detail, "Cash out");
    } else if (t == TRANSACTION_TYPE_SEND) {
        const char *raw = description;
        if (strncmp(description, "Send:", 5) == 0) raw = description + 5;
        while (*raw == ' ') ++raw;

        char recipient[256];
        const char *bar = strchr(raw, '|');
        if (bar) {
            size_t len = (size_t)(bar - raw);
            if (len >= sizeof recipient) len = sizeof recipient - 1;
            memcpy(recipient, raw, len);
            recipient[len] = '\0';
        } else {
            strncpy(recipient, raw, sizeof recipient - 1);
            recipient[sizeof recipient - 1] = '\0';
        }
        for (char *c = recipient + strlen(recipient) - 1; c >= recipient && *c == ' '; --c) *c = '\0';

        snprintf(label, sizeof label, "Transfer – %s", recipient);

        const char *note = strstr(raw, "note:");
        if (note) {
            note += 5;
            while (*note == ' ') ++note;
            strncpy(detail, note, sizeof detail - 1);
            detail[sizeof detail - 1] = '\0';
            char *note_bar = strchr(detail, '|');
            if (note_bar) *note_bar = '\0';
        } else {
            snprintf(detail, sizeof detail, "%s", "Rent share");
        }
    } else if (t == TRANSACTION_TYPE_TRANSFER) {
        snprintf(label, sizeof label, "Transfer");
        snprintf(detail, sizeof detail, "%s", description);
    } else if (t == TRANSACTION_TYPE_DEPOSIT) {
        if (strstr(description, "Salary") || strstr(description, "salary")) {
            snprintf(label, sizeof label, "Salary");
            snprintf(detail, sizeof detail, "Gentlix Bank");
        } else if (strstr(description, "Opening balance")) {
            snprintf(label, sizeof label, "Balance adjustment");
            snprintf(detail, sizeof detail, "%s", description);
        } else {
            snprintf(label, sizeof label, "Deposit");
            snprintf(detail, sizeof detail, "%s", description);
        }
    }

    rfc3339_utc(transaction_recorded_on(tx), transaction_recorded_on_micros(tx),
                out->recorded_on, sizeof out->recorded_on);

    out->id = transaction_id(tx).value;
    snprintf(out->label, sizeof out->label, "%s", label);
    snprintf(out->description, sizeof out->description, "%s", detail);
    out->amount_cents = amount_cents;
    snprintf(out->category, sizeof out->category, "%s",
             ui_category(t, pay_category, description));
    out->is_income = amount_cents > 0;
}

static int days_in_month(int year, int month) {
    static const int mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int d = mdays[month - 1];
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (leap) d = 29;
    }
    return d;
}

DashboardService *dashboard_service_new(
    UserService *user_svc,
    TransactionService *tx_svc,
    AffiliateService *affiliate_svc
) {
    DashboardService *svc = (DashboardService *)calloc(1, sizeof(DashboardService));
    if (!svc) return NULL;
    svc->user_svc = user_svc;
    svc->tx_svc = tx_svc;
    svc->affiliate_svc = affiliate_svc;
    return svc;
}

void dashboard_service_free(DashboardService *svc) {
    free(svc);
}

void dashboard_data_free(DashboardData *data) {
    if (!data) return;
    free(data->recent_activity);
    free(data->spending.daily_cumulative);
    memset(data, 0, sizeof *data);
}

bool dashboard_service_get(
    DashboardService *svc,
    UserId user_id,
    DashboardData *out,
    ServiceError *err
) {
    if (!svc || !out) {
        if (err) *err = service_error_validation("dashboard_service_get: invalid arguments");
        return false;
    }
    memset(out, 0, sizeof *out);

    UserWithAccounts dto;
    if (!user_service_get_user_with_accounts(svc->user_svc, user_id, &dto, err)) {
        return false;
    }

    int64_t total_balance_cents = 0;
    AccountId *owned = NULL;
    if (dto.account_count > 0) {
        owned = (AccountId *)calloc(dto.account_count, sizeof(AccountId));
        if (!owned) {
            free_user_with_accounts(&dto);
        if (err) *err = service_error_internal("out of memory");
            return false;
        }
    }
    for (size_t i = 0; i < dto.account_count; ++i) {
        total_balance_cents += account_balance_cents(dto.accounts[i]);
        if (owned) owned[i] = account_id(dto.accounts[i]);
    }

    ListAffiliatesParams aparams;
    memset(&aparams, 0, sizeof aparams);
    aparams.has_page = true;
    aparams.page = 1;
    aparams.has_page_size = true;
    aparams.page_size = 1;

    PaginatedAffiliatesView affiliates;
    if (!affiliate_service_list_affiliates_view(
            svc->affiliate_svc, user_id, &aparams, &affiliates, err)) {
        free(owned);
        free_user_with_accounts(&dto);
        return false;
    }

    time_t now = time(NULL);
    struct tm now_tm;
#ifdef _WIN32
    gmtime_s(&now_tm, &now);
#else
    gmtime_r(&now, &now_tm);
#endif
    int cur_year  = now_tm.tm_year + 1900;
    int cur_month = now_tm.tm_mon + 1;

    int prev_year  = (cur_month == 1) ? cur_year - 1 : cur_year;
    int prev_month = (cur_month == 1) ? 12 : cur_month - 1;

    int next_year  = (cur_month == 12) ? cur_year + 1 : cur_year;
    int next_month = (cur_month == 12) ? 1 : cur_month + 1;
    time_t month_start      = utc_civil_to_time_h(cur_year,  cur_month,  1, 0, 0, 0);
    time_t next_month_start = utc_civil_to_time_h(next_year, next_month, 1, 0, 0, 0);
    time_t month_end        = next_month_start - 1;
    time_t prev_start       = utc_civil_to_time_h(prev_year, prev_month, 1, 0, 0, 0);
    int prev_next_month     = (prev_month == 12) ? 1 : prev_month + 1;
    int prev_next_year      = (prev_month == 12) ? prev_year + 1 : prev_year;
    time_t prev_end         = utc_civil_to_time_h(prev_next_year, prev_next_month, 1, 0, 0, 0) - 1;

    // Stats for current and previous UTC month (MoM comparison).
    UserTransactionStatistics cur_stats, prev_stats;
    if (!transaction_service_compute_user_statistics(
            svc->tx_svc, user_id, 500, true, month_start, true, month_end, &cur_stats, err)) {
        paginated_affiliates_view_free(&affiliates);
        free(owned);
        free_user_with_accounts(&dto);
        return false;
    }
    if (!transaction_service_compute_user_statistics(
            svc->tx_svc, user_id, 500, true, prev_start, true, prev_end, &prev_stats, err)) {
        user_transaction_statistics_free(&cur_stats);
        paginated_affiliates_view_free(&affiliates);
        free(owned);
        free_user_with_accounts(&dto);
        return false;
    }

    int64_t net_change = cur_stats.total_incoming_cents - cur_stats.total_outgoing_cents;
    // Implied balance at month start = today's total minus this month's net flow.
    int64_t balance_at_start = total_balance_cents - net_change;
    double balance_change_percent = percent_change(net_change, balance_at_start);

    int64_t transfers_total_cents = 0;
    if (cur_stats.per_type.present[TRANSACTION_TYPE_SEND])
        transfers_total_cents += cur_stats.per_type.totals[TRANSACTION_TYPE_SEND];
    if (cur_stats.per_type.present[TRANSACTION_TYPE_TRANSFER])
        transfers_total_cents += cur_stats.per_type.totals[TRANSACTION_TYPE_TRANSFER];


    // 1. Collect and dedupe recent transactions across all accounts.
    size_t tx_cap = 0, tx_count = 0;
    TxSortEntry *tx_entries = NULL;

    for (size_t i = 0; i < dto.account_count; ++i) {
        Transaction **txs = NULL;
        size_t count = 0;
        AccountId aid = account_id(dto.accounts[i]);
        if (!transaction_service_list_recent_for_user(
                svc->tx_svc, user_id, aid, 100, 0, &txs, &count, err)) {
            for (size_t k = 0; k < tx_count; ++k) transaction_free(tx_entries[k].tx);
            free(tx_entries);
            user_transaction_statistics_free(&cur_stats);
            user_transaction_statistics_free(&prev_stats);
            paginated_affiliates_view_free(&affiliates);
            free(owned);
            free_user_with_accounts(&dto);
            return false;
        }
        for (size_t j = 0; j < count; ++j) {
            int64_t tid = transaction_id(txs[j]).value;
            bool dup = false;
            for (size_t k = 0; k < tx_count; ++k) {
                if (tx_entries[k].id == tid) { dup = true; break; }
            }
            if (dup) { transaction_free(txs[j]); continue; }

            if (tx_count >= tx_cap) {
                size_t new_cap = tx_cap == 0 ? 16 : tx_cap * 2;
                TxSortEntry *tmp = (TxSortEntry *)realloc(tx_entries, new_cap * sizeof(TxSortEntry));
                if (!tmp) {
                    transaction_free(txs[j]);
                    for (size_t k = 0; k < tx_count; ++k) transaction_free(tx_entries[k].tx);
                    free(tx_entries);
                    for (size_t k = j + 1; k < count; ++k) transaction_free(txs[k]);
                    free(txs);
                    user_transaction_statistics_free(&cur_stats);
                    user_transaction_statistics_free(&prev_stats);
                    paginated_affiliates_view_free(&affiliates);
                    free(owned);
                    free_user_with_accounts(&dto);
                    ServiceError oom = service_error_internal("out of memory collecting dashboard transactions");
                    if (err) *err = oom;
                    return false;
                }
                tx_entries = tmp;
                tx_cap = new_cap;
            }
            tx_entries[tx_count].tx = txs[j];
            tx_entries[tx_count].id = tid;
            ++tx_count;
        }
        free(txs);
    }

    if (tx_entries) qsort(tx_entries, tx_count, sizeof(TxSortEntry), tx_sort_desc);

    // 2. Build spending chart (payments + withdrawals only).
    int dim = days_in_month(cur_year, cur_month);
    int today_day = now_tm.tm_mday;
    int plot_dim = today_day < dim ? today_day : dim;

    int64_t *day_spending = (int64_t *)calloc((size_t)dim + 1, sizeof(int64_t));
    if (!day_spending) {
        for (size_t i = 0; i < tx_count; ++i) transaction_free(tx_entries[i].tx);
        free(tx_entries);
        user_transaction_statistics_free(&cur_stats);
        user_transaction_statistics_free(&prev_stats);
        paginated_affiliates_view_free(&affiliates);
        free(owned);
        free_user_with_accounts(&dto);
        ServiceError e = service_error_internal("out of memory");
        if (err) *err = e;
        return false;
    }

    for (size_t i = 0; i < tx_count; ++i) {
        Transaction *tx = tx_entries[i].tx;
        time_t recorded = transaction_recorded_on(tx);
        if (recorded < month_start || recorded > month_end) continue;

        TransactionType t = transaction_type_get(tx);
        if (t != TRANSACTION_TYPE_PAYMENT && t != TRANSACTION_TYPE_WITHDRAWAL) continue;

        bool from_owned = false;
        AccountId from = transaction_from_account_id(tx);
        for (size_t o = 0; o < dto.account_count; ++o) {
            if (owned[o].value == from.value) { from_owned = true; break; }
        }
        if (!from_owned) continue;

        struct tm rtm;
#ifdef _WIN32
        gmtime_s(&rtm, &recorded);
#else
        gmtime_r(&recorded, &rtm);
#endif
        if (rtm.tm_year + 1900 == cur_year && rtm.tm_mon + 1 == cur_month) {
            int d = rtm.tm_mday;
            if (d >= 1 && d <= dim) day_spending[d] += transaction_value_cents(tx);
        }
    }

    int64_t cumulative = 0;
    int64_t total_spent_cents = 0;
    DashboardDailySpendingPoint *daily = NULL;
    if (plot_dim > 0) {
        daily = (DashboardDailySpendingPoint *)calloc((size_t)plot_dim, sizeof(DashboardDailySpendingPoint));
        if (!daily) {
            for (size_t i = 0; i < tx_count; ++i) transaction_free(tx_entries[i].tx);
            free(tx_entries);
            free(day_spending);
            user_transaction_statistics_free(&cur_stats);
            user_transaction_statistics_free(&prev_stats);
            paginated_affiliates_view_free(&affiliates);
            free(owned);
            free_user_with_accounts(&dto);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
    }

    size_t daily_count = 0;
    for (int day = 1; day <= plot_dim; ++day) {
        cumulative += day_spending[day];
        time_t day_t = utc_civil_to_time_h(cur_year, cur_month, day, 12, 0, 0);
        struct tm day_tm;
#ifdef _WIN32
        gmtime_s(&day_tm, &day_t);
#else
        gmtime_r(&day_t, &day_tm);
#endif

        snprintf(daily[daily_count].date, sizeof daily[daily_count].date,
                 "%04d-%02d-%02d", cur_year, cur_month, day);
        snprintf(daily[daily_count].day_label, sizeof daily[daily_count].day_label,
                 "%s %d", weekday_short(day_tm.tm_wday), day);
        daily[daily_count].cumulative_spending_cents = cumulative;
        ++daily_count;
    }
    total_spent_cents = cumulative;
    free(day_spending);

    int64_t current_spending = 0;
    int64_t prev_spending = 0;
    if (cur_stats.per_type.present[TRANSACTION_TYPE_PAYMENT])
        current_spending += cur_stats.per_type.totals[TRANSACTION_TYPE_PAYMENT];
    if (cur_stats.per_type.present[TRANSACTION_TYPE_WITHDRAWAL])
        current_spending += cur_stats.per_type.totals[TRANSACTION_TYPE_WITHDRAWAL];
    if (prev_stats.per_type.present[TRANSACTION_TYPE_PAYMENT])
        prev_spending += prev_stats.per_type.totals[TRANSACTION_TYPE_PAYMENT];
    if (prev_stats.per_type.present[TRANSACTION_TYPE_WITHDRAWAL])
        prev_spending += prev_stats.per_type.totals[TRANSACTION_TYPE_WITHDRAWAL];

    double spending_change = percent_change(current_spending, prev_spending);

    enum { RECENT_ACTIVITY_LIMIT = 5 };
    DashboardActivityItem *recent = NULL;
    size_t recent_count = 0;
    if (RECENT_ACTIVITY_LIMIT > 0) {
        recent = (DashboardActivityItem *)calloc(RECENT_ACTIVITY_LIMIT, sizeof(DashboardActivityItem));
        if (!recent) {
            free(daily);
            for (size_t i = 0; i < tx_count; ++i) transaction_free(tx_entries[i].tx);
            free(tx_entries);
            user_transaction_statistics_free(&cur_stats);
            user_transaction_statistics_free(&prev_stats);
            paginated_affiliates_view_free(&affiliates);
            free(owned);
            free_user_with_accounts(&dto);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
    }
    for (size_t i = 0; i < tx_count && recent_count < RECENT_ACTIVITY_LIMIT; ++i) {
        map_activity_item(tx_entries[i].tx, owned, dto.account_count, &recent[recent_count]);
        ++recent_count;
    }

    for (size_t i = 0; i < tx_count; ++i) transaction_free(tx_entries[i].tx);
    free(tx_entries);

    out->total_balance_cents = total_balance_cents;
    out->balance_change_percent = balance_change_percent;
    out->active_accounts_count = (uint32_t)dto.account_count;
    out->affiliates_count = affiliates.total;
    out->transfers_total_cents = transfers_total_cents;
    out->recent_activity = recent;
    out->recent_count = recent_count;
    out->spending.total_spent_cents = total_spent_cents;
    out->spending.change_percent_vs_last_month = spending_change;
    out->spending.daily_cumulative = daily;
    out->spending.daily_count = daily_count;

    user_transaction_statistics_free(&cur_stats);
    user_transaction_statistics_free(&prev_stats);
    paginated_affiliates_view_free(&affiliates);
    free(owned);
    free_user_with_accounts(&dto);

    if (err) *err = service_error_ok();
    return true;
}
