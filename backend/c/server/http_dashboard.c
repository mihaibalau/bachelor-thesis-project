#include "include/http_dashboard.h"
#include "include/http_util.h"
#include "include/http_auth.h"
#include "service/include/dashboard_service.h"

#include <microhttpd.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>

static json_t *activity_to_json(const DashboardActivityItem *item) {
    json_t *o = json_object();
    json_object_set_new(o, "id", json_integer((json_int_t)item->id));
    json_object_set_new(o, "label", json_string(item->label));
    json_object_set_new(o, "description", json_string(item->description));
    json_object_set_new(o, "recorded_on", json_string(item->recorded_on));
    json_object_set_new(o, "amount_cents", json_integer((json_int_t)item->amount_cents));
    json_object_set_new(o, "category", json_string(item->category));
    json_object_set_new(o, "is_income", json_boolean(item->is_income));
    return o;
}

static enum MHD_Result handle_dashboard(AppState *state, struct MHD_Connection *conn) {
    AuthClaims claims;
    ServiceError serr;
    if (!http_require_auth(conn, state, &claims, &serr)) {
        return http_send_json(conn, MHD_HTTP_UNAUTHORIZED,
            "{\"status\":401,\"code\":\"unauthorized\",\"message\":\"missing or invalid token\"}");
    }

    DashboardData data;
    if (!dashboard_service_get(state->dashboard_svc, claims.sub, &data, &serr)) {
        return http_send_service_error(conn, &serr);
    }

    json_t *recent = json_array();
    for (size_t i = 0; i < data.recent_count; ++i) {
        json_array_append_new(recent, activity_to_json(&data.recent_activity[i]));
    }

    json_t *daily = json_array();
    for (size_t i = 0; i < data.spending.daily_count; ++i) {
        const DashboardDailySpendingPoint *pt = &data.spending.daily_cumulative[i];
        json_t *o = json_object();
        json_object_set_new(o, "day_label", json_string(pt->day_label));
        json_object_set_new(o, "date", json_string(pt->date));
        json_object_set_new(o, "cumulative_spending_cents",
            json_integer((json_int_t)pt->cumulative_spending_cents));
        json_array_append_new(daily, o);
    }

    json_t *spending = json_object();
    json_object_set_new(spending, "total_spent_cents",
        json_integer((json_int_t)data.spending.total_spent_cents));
    json_object_set_new(spending, "change_percent_vs_last_month",
        json_real(data.spending.change_percent_vs_last_month));
    json_object_set_new(spending, "daily_cumulative", daily);

    json_t *root = json_object();
    json_object_set_new(root, "total_balance_cents", json_integer((json_int_t)data.total_balance_cents));
    json_object_set_new(root, "balance_change_percent", json_real(data.balance_change_percent));
    json_object_set_new(root, "active_accounts_count", json_integer((json_int_t)data.active_accounts_count));
    json_object_set_new(root, "affiliates_count", json_integer((json_int_t)data.affiliates_count));
    json_object_set_new(root, "transfers_total_cents", json_integer((json_int_t)data.transfers_total_cents));
    json_object_set_new(root, "recent_activity", recent);
    json_object_set_new(root, "spending", spending);

    dashboard_data_free(&data);

    char *s = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    enum MHD_Result ret;
    if (s) { ret = http_send_json(conn, MHD_HTTP_OK, s); free(s); }
    else   { ServiceError e = service_error_internal("json serialization failed");
             ret = http_send_service_error(conn, &e); }
    return ret;
}

enum MHD_Result http_dashboard_dispatch(
    AppState *state,
    struct MHD_Connection *conn,
    const char *subpath,
    const char *method
) {
    if (strcmp(method, MHD_HTTP_METHOD_OPTIONS) == 0) {
        return http_send_empty(conn, MHD_HTTP_NO_CONTENT);
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0) {
        if (!subpath || strcmp(subpath, "") == 0 || strcmp(subpath, "/") == 0) {
            return handle_dashboard(state, conn);
        }
    }

    return http_send_not_found(conn);
}
