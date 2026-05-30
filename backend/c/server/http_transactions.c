#include "include/http_transactions.h"
#include "include/http_util.h"
#include "include/http_auth.h"
#include "service/include/transaction_service.h"
#include "domain/include/transaction.h"
#include "domain/include/transaction_type.h"
#include "domain/include/ids.h"

#include <microhttpd.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/*
 * Transactions routes. The server layer only parses the request, extracts the
 * authenticated user, calls the service, and serializes the result. Every
 * business rule lives in TransactionService.
 */

/* Format a time_t as RFC3339 in UTC (whole seconds), matching chrono's
 * DateTime<Utc>::to_rfc3339() for sub-second-free timestamps. */
static void rfc3339_utc(time_t t, char *buf, size_t buf_size) {
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
}

/* Civil UTC date -> time_t via days-from-civil (Howard Hinnant's algorithm),
 * mirroring the service's utc_civil_to_time so the current-month window here
 * matches the [start, end] bounds the Rust route computes with chrono. */
static time_t utc_civil_to_time_h(int year, int month, int day,
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

static enum MHD_Result send_transaction_id(struct MHD_Connection *conn, TransactionId id) {
    /* Rust returns Json(TransactionId) where TransactionId is a serde newtype,
     * i.e. a bare JSON integer. */
    char buf[32];
    snprintf(buf, sizeof buf, "%lld", (long long)id.value);
    return http_send_json(conn, MHD_HTTP_OK, buf);
}

// ── POST handlers ─────────────────────────────────────────────────────────

static enum MHD_Result handle_deposit(AppState *state, struct MHD_Connection *conn,
                                      AuthClaims claims, json_t *root) {
    json_t *acc_j = json_object_get(root, "account_id");
    json_t *amt_j = json_object_get(root, "amount");
    if (!json_is_integer(acc_j) || !json_is_integer(amt_j)) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"account_id and amount are required\"}");
    }
    AccountId account_id = { (int64_t)json_integer_value(acc_j) };
    TransactionId id;
    ServiceError serr;
    if (!transaction_service_record_deposit_for_user(
            state->tx_svc, claims.sub, account_id,
            (int64_t)json_integer_value(amt_j), &id, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return send_transaction_id(conn, id);
}

static enum MHD_Result handle_withdrawal(AppState *state, struct MHD_Connection *conn,
                                         AuthClaims claims, json_t *root) {
    json_t *acc_j = json_object_get(root, "account_id");
    json_t *amt_j = json_object_get(root, "amount");
    if (!json_is_integer(acc_j) || !json_is_integer(amt_j)) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"account_id and amount are required\"}");
    }
    AccountId account_id = { (int64_t)json_integer_value(acc_j) };
    TransactionId id;
    ServiceError serr;
    if (!transaction_service_record_withdrawal_for_user(
            state->tx_svc, claims.sub, account_id,
            (int64_t)json_integer_value(amt_j), &id, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return send_transaction_id(conn, id);
}

static enum MHD_Result handle_send(AppState *state, struct MHD_Connection *conn,
                                   AuthClaims claims, json_t *root) {
    json_t *from_j = json_object_get(root, "from_account_id");
    json_t *to_j   = json_object_get(root, "recipient_account_id");
    json_t *val_j  = json_object_get(root, "value_cents");
    const char *message = json_string_value(json_object_get(root, "message"));
    if (!json_is_integer(from_j) || !json_is_integer(to_j) ||
        !json_is_integer(val_j) || !message) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"from_account_id, recipient_account_id, value_cents and message are required\"}");
    }
    AccountId from_id = { (int64_t)json_integer_value(from_j) };
    AccountId to_id   = { (int64_t)json_integer_value(to_j) };
    TransactionId id;
    ServiceError serr;
    if (!transaction_service_record_send_for_user(
            state->tx_svc, claims.sub, from_id, to_id,
            (int64_t)json_integer_value(val_j), message, &id, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return send_transaction_id(conn, id);
}

static enum MHD_Result handle_transfer(AppState *state, struct MHD_Connection *conn,
                                       AuthClaims claims, json_t *root) {
    json_t *from_j = json_object_get(root, "from_account_id");
    json_t *to_j   = json_object_get(root, "to_account_id");
    json_t *val_j  = json_object_get(root, "value_cents");
    if (!json_is_integer(from_j) || !json_is_integer(to_j) || !json_is_integer(val_j)) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"from_account_id, to_account_id and value_cents are required\"}");
    }
    AccountId from_id = { (int64_t)json_integer_value(from_j) };
    AccountId to_id   = { (int64_t)json_integer_value(to_j) };
    TransactionId id;
    ServiceError serr;
    if (!transaction_service_record_transfer_for_user(
            state->tx_svc, claims.sub, from_id, to_id,
            (int64_t)json_integer_value(val_j), &id, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return send_transaction_id(conn, id);
}

static enum MHD_Result handle_payment(AppState *state, struct MHD_Connection *conn,
                                      AuthClaims claims, json_t *root) {
    json_t *from_j = json_object_get(root, "from_account_id");
    json_t *amt_j  = json_object_get(root, "amount");
    const char *category      = json_string_value(json_object_get(root, "category"));
    const char *merchant_name = json_string_value(json_object_get(root, "merchant_name"));
    const char *note          = json_string_value(json_object_get(root, "note")); /* opt */
    if (!json_is_integer(from_j) || !json_is_integer(amt_j) || !category || !merchant_name) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\","
            "\"message\":\"from_account_id, amount, category and merchant_name are required\"}");
    }
    AccountId from_id = { (int64_t)json_integer_value(from_j) };
    TransactionId id;
    ServiceError serr;
    if (!transaction_service_record_payment_for_user(
            state->tx_svc, claims.sub, from_id,
            (int64_t)json_integer_value(amt_j), category, merchant_name, note,
            &id, &serr)) {
        return http_send_service_error(conn, &serr);
    }
    return send_transaction_id(conn, id);
}

// Dispatch a fully-buffered POST body to the matching handler.
static enum MHD_Result dispatch_post(AppState *state, struct MHD_Connection *conn,
                                     const char *subpath, const char *body) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\","
            "\"message\":\"missing or invalid token\"}");
    }

    json_error_t jerr;
    json_t *root = json_loads(body, 0, &jerr);
    if (!root) {
        return http_send_json(conn, MHD_HTTP_BAD_REQUEST,
            "{\"status\":400,\"code\":\"bad_request\",\"message\":\"invalid JSON body\"}");
    }

    enum MHD_Result result;
    if      (strcmp(subpath, "/deposit")    == 0) result = handle_deposit(state, conn, claims, root);
    else if (strcmp(subpath, "/withdrawal") == 0) result = handle_withdrawal(state, conn, claims, root);
    else if (strcmp(subpath, "/send")       == 0) result = handle_send(state, conn, claims, root);
    else if (strcmp(subpath, "/transfer")   == 0) result = handle_transfer(state, conn, claims, root);
    else if (strcmp(subpath, "/payment")    == 0) result = handle_payment(state, conn, claims, root);
    else                                          result = http_send_not_found(conn);

    json_decref(root);
    return result;
}

/* ── GET handlers ────────────────────────────────────────────────────────── */

static json_t *transaction_to_json(const Transaction *tx) {
    char recorded[40];
    rfc3339_utc(transaction_recorded_on(tx), recorded, sizeof recorded);

    json_t *o = json_object();
    json_object_set_new(o, "id",               json_integer((json_int_t)transaction_id(tx).value));
    json_object_set_new(o, "from_account_id",  json_integer((json_int_t)transaction_from_account_id(tx).value));
    json_object_set_new(o, "to_account_id",    json_integer((json_int_t)transaction_to_account_id(tx).value));
    json_object_set_new(o, "transaction_type", json_string(transaction_type_as_str(transaction_type_get(tx))));
    json_object_set_new(o, "value_cents",      json_integer((json_int_t)transaction_value_cents(tx)));
    json_object_set_new(o, "recorded_on",      json_string(recorded));
    json_object_set_new(o, "description",      json_string(transaction_description(tx)));
    return o;
}

static enum MHD_Result handle_recent(AppState *state, struct MHD_Connection *conn) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\",\"message\":\"missing or invalid token\"}");
    }

    const char *account_id_s = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "account_id");
    const char *limit_s      = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "limit");
    if (!account_id_s) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\",\"message\":\"account_id is required\"}");
    }

    AccountId account_id = { (int64_t)strtoll(account_id_s, NULL, 10) };
    int64_t limit = limit_s ? (int64_t)strtoll(limit_s, NULL, 10) : 0; /* 0 → service default 10 */

    Transaction **txs = NULL;
    size_t count = 0;
    if (!transaction_service_list_recent_for_user(
            state->tx_svc, claims.sub, account_id, limit, &txs, &count, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *items = json_array();
    for (size_t i = 0; i < count; ++i) {
        json_array_append_new(items, transaction_to_json(txs[i]));
        transaction_free(txs[i]);
    }
    free(txs);

    json_t *root = json_object();
    json_object_set_new(root, "items", items);
    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

static enum MHD_Result handle_statement(AppState *state, struct MHD_Connection *conn) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\",\"message\":\"missing or invalid token\"}");
    }

    const char *account_id_s = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "account_id");
    const char *from_s       = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "from");
    const char *to_s         = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "to");
    const char *limit_s      = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "limit");
    const char *offset_s     = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "offset");
    if (!account_id_s) {
        return http_send_json(conn, MHD_HTTP_UNPROCESSABLE_ENTITY,
            "{\"status\":422,\"code\":\"validation_error\",\"message\":\"account_id is required\"}");
    }

    AccountId account_id = { (int64_t)strtoll(account_id_s, NULL, 10) };
    int64_t limit  = limit_s  ? (int64_t)strtoll(limit_s, NULL, 10)  : 0;  /* 0 → service default 100 */
    int64_t offset = offset_s ? (int64_t)strtoll(offset_s, NULL, 10) : 0;

    AccountStatementEntry *entries = NULL;
    size_t count = 0;
    if (!transaction_service_compute_account_statement_for_user_from_strings(
            state->tx_svc, claims.sub, account_id, from_s, to_s, limit, offset,
            &entries, &count, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *items = json_array();
    for (size_t i = 0; i < count; ++i) {
        char recorded[40];
        rfc3339_utc(entries[i].recorded_on, recorded, sizeof recorded);
        json_t *o = json_object();
        json_object_set_new(o, "transaction_id",      json_integer((json_int_t)entries[i].transaction_id.value));
        json_object_set_new(o, "recorded_on",         json_string(recorded));
        json_object_set_new(o, "description",         json_string(entries[i].description));
        json_object_set_new(o, "transaction_type",    json_string(transaction_type_as_str(entries[i].transaction_type)));
        json_object_set_new(o, "value_cents",         json_integer((json_int_t)entries[i].value_cents));
        json_object_set_new(o, "balance_after_cents", json_integer((json_int_t)entries[i].balance_after_cents));
        json_array_append_new(items, o);
    }
    free(entries);

    json_t *root = json_object();
    json_object_set_new(root, "items", items);
    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

static enum MHD_Result handle_monthly_summary(AppState *state, struct MHD_Connection *conn) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\",\"message\":\"missing or invalid token\"}");
    }

    const char *limit_s = MHD_lookup_connection_value(conn, MHD_GET_ARGUMENT_KIND, "per_account_limit");
    int64_t per_account_limit = limit_s ? (int64_t)strtoll(limit_s, NULL, 10) : 500; /* Rust unwrap_or(500) */

    /* Current-month "shaping" lives in this layer on purpose (see file header).
     * We compute the [start, end] bounds of the current calendar month (UTC) and
     * pass them to the service so EVERY aggregate — totals, per-type and the
     * daily series — reflects only the current month, exactly like the Rust route. */
    time_t now = time(NULL);
    struct tm now_tm;
#ifdef _WIN32
    gmtime_s(&now_tm, &now);
#else
    gmtime_r(&now, &now_tm);
#endif
    int cur_year  = now_tm.tm_year + 1900;
    int cur_month = now_tm.tm_mon + 1;

    int next_year  = (cur_month == 12) ? cur_year + 1 : cur_year;
    int next_month = (cur_month == 12) ? 1 : cur_month + 1;
    time_t month_start      = utc_civil_to_time_h(cur_year,  cur_month,  1, 0, 0, 0);
    time_t next_month_start = utc_civil_to_time_h(next_year, next_month, 1, 0, 0, 0);
    time_t month_end        = next_month_start - 1;

    UserTransactionStatistics stats;
    if (!transaction_service_compute_user_statistics(
            state->tx_svc, claims.sub, per_account_limit,
            true, month_start, true, month_end, &stats, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *per_type = json_array();
    for (size_t t = 0; t < TX_SERVICE_TYPE_COUNT; ++t) {
        if (stats.per_type.present[t]) {
            json_t *o = json_object();
            json_object_set_new(o, "transaction_type",
                json_string(transaction_type_as_str((TransactionType)t)));
            json_object_set_new(o, "total_cents",
                json_integer((json_int_t)stats.per_type.totals[t]));
            json_array_append_new(per_type, o);
        }
    }

    // daily_cumulative_spending — only the current UTC month, cumulative
    json_t *daily = json_array();
    int64_t cumulative = 0;
    for (size_t i = 0; i < stats.per_day_count; ++i) {
        int32_t key = stats.per_day[i].date_key; // YYYYMMDD, already sorted asc
        int y = key / 10000;
        int m = (key / 100) % 100;
        int d = key % 100;
        if (y != cur_year || m != cur_month) continue;

        int64_t value = stats.per_day[i].total;
        int64_t spending = value < 0 ? -value : 0; // value.min(0).abs()
        cumulative += spending;

        char date_str[11];
        snprintf(date_str, sizeof date_str, "%04d-%02d-%02d", y, m, d);

        json_t *o = json_object();
        json_object_set_new(o, "date",                      json_string(date_str));
        json_object_set_new(o, "spending_cents",            json_integer((json_int_t)spending));
        json_object_set_new(o, "cumulative_spending_cents", json_integer((json_int_t)cumulative));
        json_array_append_new(daily, o);
    }

    json_t *root = json_object();
    json_object_set_new(root, "total_incoming_cents",      json_integer((json_int_t)stats.total_incoming_cents));
    json_object_set_new(root, "total_outgoing_cents",      json_integer((json_int_t)stats.total_outgoing_cents));
    json_object_set_new(root, "total_volume_cents",        json_integer((json_int_t)stats.total_volume_cents));
    json_object_set_new(root, "per_type_totals",           per_type);
    json_object_set_new(root, "daily_cumulative_spending", daily);

    user_transaction_statistics_free(&stats);

    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

// ── Dispatcher ─────────────────────────────────────────────────────────

enum MHD_Result http_transactions_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls
) {
    if (strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0) {
        return http_send_empty(conn, MHD_HTTP_NO_CONTENT);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if      (subpath && strcmp(subpath, "/recent")          == 0) return handle_recent(state, conn);
        else if (subpath && strcmp(subpath, "/statement")       == 0) return handle_statement(state, conn);
        else if (subpath && strcmp(subpath, "/summary/monthly") == 0) return handle_monthly_summary(state, conn);
        return http_send_not_found(conn);
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
        if (*con_cls == NULL) {
            BodyBuffer *bb = body_buffer_new();
            if (!bb) return MHD_NO;
            *con_cls = bb;
            return MHD_YES;
        }
        BodyBuffer *bb = (BodyBuffer *)*con_cls;
        if (*upload_data_size > 0) {
            if (!body_buffer_append(bb, upload_data, *upload_data_size)) {
                body_buffer_free(bb);
                *con_cls = NULL;
                return MHD_NO;
            }
            *upload_data_size = 0;
            return MHD_YES;
        }

        const char *body = bb->buf ? bb->buf : "";
        const char *sp   = subpath ? subpath : "";
        enum MHD_Result result = dispatch_post(state, conn, sp, body);

        body_buffer_free(bb);
        *con_cls = NULL;
        return result;
    }

    return http_send_not_found(conn);
}
