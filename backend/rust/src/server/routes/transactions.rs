use std::sync::Arc;

use axum::{
    extract::{Query, State},
    middleware,
    routing::{get, post},
    Extension, Json, Router,
};
use chrono::{NaiveDate, Utc};
use serde::{Deserialize, Serialize};

use crate::{
    domain::{
        ids::{AccountId, TransactionId, UserId},
        value::transaction_type::TransactionType,
    },
    server::{
        auth::require_auth,
        error::{ApiError, ApiResult},
        state::AppState,
    },
    service::{
        auth::Claims,
        errors::ServiceError,
        transaction::{ExtendedUserStatistics, StatisticsFilters},
    },
};
use std::str::FromStr;

#[derive(Deserialize)]
pub struct DepositRequest {
    pub account_id: i64,
    pub amount: i64, // fără cenți
}

#[derive(Deserialize)]
pub struct WithdrawalRequest {
    pub account_id: i64,
    pub amount: i64,
}

#[derive(Deserialize)]
pub struct SendRequest {
    pub from_account_id: i64,
    pub recipient_account_id: i64, // de ex. din affiliate
    pub value_cents: i64,
    pub message: String,
}

#[derive(Deserialize)]
pub struct TransferRequest {
    pub from_account_id: i64,
    pub to_account_id: i64,
    pub value_cents: i64,
}

#[derive(Deserialize)]
pub struct PaymentRequest {
    pub from_account_id: i64,
    pub amount: i64, // fără cenți
    pub category: String,
    pub merchant_name: String,
    pub note: Option<String>,
}

#[derive(Deserialize)]
pub struct RecentTransactionsQuery {
    pub account_id: i64,
    pub limit: Option<i64>,
    pub offset: Option<i64>,
}

#[derive(Deserialize)]
pub struct AccountStatementQueryDto {
    pub account_id: i64,
    pub from: Option<String>, // YYYY-MM-DD
    pub to: Option<String>,   // YYYY-MM-DD
    pub limit: Option<i64>,
    pub offset: Option<i64>,
}

#[derive(Deserialize)]
pub struct UserMonthlySummaryQuery {
    pub per_account_limit: Option<i64>,
}

#[derive(Deserialize)]
pub struct TransactionSummaryQuery {
    pub from: Option<String>,
    pub to: Option<String>,
    pub account_id: Option<i64>,
    pub transaction_type: Option<String>,
    pub per_account_limit: Option<i64>,
}

#[derive(Serialize)]
pub struct TransactionResponse {
    pub id: i64,
    pub from_account_id: i64,
    pub to_account_id: i64,
    pub transaction_type: String,
    pub value_cents: i64,
    pub recorded_on: String,
    pub description: String,
}

#[derive(Serialize)]
pub struct AccountStatementEntryResponse {
    pub transaction_id: i64,
    pub recorded_on: String,
    pub description: String,
    pub transaction_type: String,
    pub value_cents: i64,
    pub balance_after_cents: i64,
}

#[derive(Serialize)]
pub struct RecentTransactionsResponse {
    pub items: Vec<TransactionResponse>,
}

#[derive(Serialize)]
pub struct AccountStatementResponse {
    pub items: Vec<AccountStatementEntryResponse>,
}

#[derive(Serialize)]
pub struct PerTypeTotalResponse {
    pub transaction_type: String,
    pub total_cents: i64,
}

#[derive(Serialize)]
pub struct DailyCumulativeSpendingPoint {
    pub date: String,                // YYYY-MM-DD
    pub spending_cents: i64,
    pub cumulative_spending_cents: i64,
}

#[derive(Serialize)]
pub struct UserMonthlySummaryResponse {
    pub total_incoming_cents: i64,
    pub total_outgoing_cents: i64,
    pub total_volume_cents: i64,
    pub per_type_totals: Vec<PerTypeTotalResponse>,
    pub daily_cumulative_spending: Vec<DailyCumulativeSpendingPoint>,
}

#[derive(Serialize)]
pub struct DailyNetPoint {
    pub date: String,
    pub net_cents: i64,
}

#[derive(Serialize)]
pub struct PaymentCategoryTotal {
    pub category: String,
    pub total_cents: i64,
}

#[derive(Serialize)]
pub struct AccountBalanceSummary {
    pub account_id: i64,
    pub account_type: String,
    pub currency: String,
    pub balance_cents: i64,
}

#[derive(Serialize)]
pub struct TransactionSummaryResponse {
    pub total_incoming_cents: i64,
    pub total_outgoing_cents: i64,
    pub net_flow_cents: i64,
    pub total_volume_cents: i64,
    pub transaction_count: i64,
    pub per_type_totals: Vec<PerTypeTotalResponse>,
    pub daily_net: Vec<DailyNetPoint>,
    pub daily_cumulative_spending: Vec<DailyCumulativeSpendingPoint>,
    pub payment_category_totals: Vec<PaymentCategoryTotal>,
    pub account_balances: Vec<AccountBalanceSummary>,
}

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    let private = Router::new()
        .route("/deposit",    post(record_deposit))
        .route("/withdrawal", post(record_withdrawal))
        .route("/send",       post(record_send))
        .route("/transfer",   post(record_transfer))
        .route("/payment",    post(record_payment))
        .route("/recent",     get(get_recent_transactions))
        .route("/statement",  get(get_account_statement))
        .route("/summary", get(get_transaction_summary))
        .route("/summary/monthly", get(get_user_monthly_summary))
        .route_layer(middleware::from_fn_with_state(
            state.clone(),
            require_auth,
        ));

    Router::new().merge(private)
}

async fn record_deposit(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<DepositRequest>,
) -> ApiResult<Json<TransactionId>> {
    let user_id = UserId::from(claims.sub);
    let account_id = AccountId::from(body.account_id);

    let id = state
        .tx_svc
        .record_deposit_for_user(user_id, account_id, body.amount)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(id))
}

async fn record_withdrawal(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<WithdrawalRequest>,
) -> ApiResult<Json<TransactionId>> {
    let user_id = UserId::from(claims.sub);

    let account_id = AccountId::from(body.account_id);

    let id = state
        .tx_svc
        .record_withdrawal_for_user(user_id, account_id, body.amount)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(id))
}

async fn record_send(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<SendRequest>,
) -> ApiResult<Json<TransactionId>> {
    let user_id = UserId::from(claims.sub);

    let from_id = AccountId::from(body.from_account_id);
    let to_id = AccountId::from(body.recipient_account_id);

    // 1. Validate affiliate send target
    state
        .affiliate_svc
        .validate_send_target(user_id, from_id, to_id)
        .await
        .map_err(ApiError::from)?;

    // 2. Delegate to service
    let id = state
        .tx_svc
        .record_send_for_user(user_id, from_id, to_id, body.value_cents, body.message)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(id))
}

async fn record_transfer(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<TransferRequest>,
) -> ApiResult<Json<TransactionId>> {
    let user_id = UserId::from(claims.sub);

    let from_id = AccountId::from(body.from_account_id);
    let to_id = AccountId::from(body.to_account_id);

    let id = state
        .tx_svc
        .record_transfer_for_user(user_id, from_id, to_id, body.value_cents)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(id))
}

async fn record_payment(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<PaymentRequest>,
) -> ApiResult<Json<TransactionId>> {
    let user_id = UserId::from(claims.sub);

    let from_id = AccountId::from(body.from_account_id);

    let id = state
        .tx_svc
        .record_payment_for_user(
            user_id,
            from_id,
            body.amount,
            body.category,
            body.merchant_name,
            body.note,
        )
        .await
        .map_err(ApiError::from)?;

    Ok(Json(id))
}

async fn get_recent_transactions(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(query): Query<RecentTransactionsQuery>,
) -> ApiResult<Json<RecentTransactionsResponse>> {
    let user_id = UserId::from(claims.sub);
    let account_id = AccountId::from(query.account_id);

    let txs = state
        .tx_svc
        .list_recent_for_user(user_id, account_id, query.limit, query.offset)
        .await
        .map_err(ApiError::from)?;

    let mut items = Vec::with_capacity(txs.len());
    for tx in txs {
        let id = tx
            .id()
            .ok_or_else(|| ApiError(ServiceError::internal("transaction missing id")))?
            .0;
        items.push(TransactionResponse {
            id,
            from_account_id: tx.from_account_id().0,
            to_account_id: tx.to_account_id().0,
            transaction_type: tx.transaction_type().as_str().to_string(),
            value_cents: tx.value_cents(),
            recorded_on: tx.recorded_on().to_rfc3339(),
            description: tx.description().to_string(),
        });
    }

    Ok(Json(RecentTransactionsResponse { items }))
}

async fn get_account_statement(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(q): Query<AccountStatementQueryDto>,
) -> ApiResult<Json<AccountStatementResponse>> {
    let user_id = UserId::from(claims.sub);
    let account_id = AccountId::from(q.account_id);

    let entries = state
        .tx_svc
        .compute_account_statement_for_user_from_strings(
            user_id,
            account_id,
            q.from.clone(),
            q.to.clone(),
            q.limit,
            q.offset,
        )
        .await
        .map_err(ApiError::from)?;

    let items = entries
        .into_iter()
        .map(|e| AccountStatementEntryResponse {
            transaction_id: e.transaction_id.0,
            recorded_on: e.recorded_on.to_rfc3339(),
            description: e.description,
            transaction_type: e.transaction_type.as_str().to_string(),
            value_cents: e.value_cents,
            balance_after_cents: e.balance_after_cents,
        })
        .collect();

    Ok(Json(AccountStatementResponse { items }))
}

fn parse_summary_date_range(
    from: Option<String>,
    to: Option<String>,
) -> Result<(chrono::DateTime<Utc>, chrono::DateTime<Utc>), ServiceError> {
    use chrono::{Datelike, NaiveDate, TimeZone};

    let now = Utc::now();
    let current_year = now.year();
    let current_month = now.month();

    let from_dt = match from {
        Some(s) => NaiveDate::parse_from_str(&s, "%Y-%m-%d")
            .map_err(|e| ServiceError::Validation(e.to_string()))?
            .and_hms_opt(0, 0, 0)
            .unwrap()
            .and_local_timezone(Utc)
            .unwrap(),
        None => Utc
            .with_ymd_and_hms(current_year, current_month, 1, 0, 0, 0)
            .unwrap(),
    };

    let to_dt = match to {
        Some(s) => NaiveDate::parse_from_str(&s, "%Y-%m-%d")
            .map_err(|e| ServiceError::Validation(e.to_string()))?
            .and_hms_opt(23, 59, 59)
            .unwrap()
            .and_local_timezone(Utc)
            .unwrap(),
        None => now,
    };

    Ok((from_dt, to_dt))
}

fn build_summary_response(
    stats: ExtendedUserStatistics,
    account_balances: Vec<AccountBalanceSummary>,
) -> TransactionSummaryResponse {
    let mut daily_points: Vec<(NaiveDate, i64)> = stats.per_day_totals.into_iter().collect();
    daily_points.sort_by_key(|(d, _)| *d);

    let daily_net: Vec<DailyNetPoint> = daily_points
        .iter()
        .map(|(date, net)| DailyNetPoint {
            date: date.to_string(),
            net_cents: *net,
        })
        .collect();

    let mut cumulative = 0;
    let daily_cumulative_spending: Vec<DailyCumulativeSpendingPoint> = daily_points
        .into_iter()
        .map(|(date, value)| {
            let spending = value.min(0).abs();
            cumulative += spending;
            DailyCumulativeSpendingPoint {
                date: date.to_string(),
                spending_cents: spending,
                cumulative_spending_cents: cumulative,
            }
        })
        .collect();

    let per_type_totals = stats
        .per_type_totals
        .into_iter()
        .filter(|(_, v)| *v > 0)
        .map(|(t, v)| PerTypeTotalResponse {
            transaction_type: t.as_str().to_string(),
            total_cents: v,
        })
        .collect();

    let payment_category_totals = stats
        .payment_category_totals
        .into_iter()
        .map(|(category, total_cents)| PaymentCategoryTotal {
            category,
            total_cents,
        })
        .collect();

    TransactionSummaryResponse {
        total_incoming_cents: stats.total_incoming_cents,
        total_outgoing_cents: stats.total_outgoing_cents,
        net_flow_cents: stats.total_incoming_cents - stats.total_outgoing_cents,
        total_volume_cents: stats.total_volume_cents,
        transaction_count: stats.transaction_count,
        per_type_totals,
        daily_net,
        daily_cumulative_spending,
        payment_category_totals,
        account_balances,
    }
}

async fn get_transaction_summary(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(q): Query<TransactionSummaryQuery>,
) -> ApiResult<Json<TransactionSummaryResponse>> {
    let user_id = UserId::from(claims.sub);
    let per_account_limit = q.per_account_limit.unwrap_or(500);

    // 1. Parse date range
    let (from_dt, to_dt) = parse_summary_date_range(q.from, q.to).map_err(ApiError::from)?;

    // 2. Build filters and delegate to service
    let accounts = state
        .account_svc
        .list_for_user(user_id)
        .await
        .map_err(ApiError::from)?;

    let mut filters = StatisticsFilters::default();

    if let Some(account_id) = q.account_id {
        let account_id = AccountId::from(account_id);
        if !accounts.iter().any(|a| a.id() == Some(account_id)) {
            return Err(ApiError(ServiceError::not_found("account")));
        }
        filters.scope_account_id = Some(account_id);
    }

    if let Some(tx_type) = q.transaction_type.as_deref() {
        if tx_type != "All" {
            filters.transaction_type = Some(
                TransactionType::from_str(tx_type)
                    .map_err(|e| ApiError(ServiceError::Validation(e.to_string())))?,
            );
        }
    }

    let stats = state
        .tx_svc
        .compute_user_statistics_extended(
            user_id,
            per_account_limit,
            Some(from_dt),
            Some(to_dt),
            filters,
        )
        .await
        .map_err(ApiError::from)?;

    // 3. Map to response DTO
    let account_balances = accounts
        .into_iter()
        .filter_map(|a| {
            Some(AccountBalanceSummary {
                account_id: a.id()?.0,
                account_type: a.account_type().as_str().to_string(),
                currency: a.currency().as_str().to_string(),
                balance_cents: a.balance_cents(),
            })
        })
        .collect();

    Ok(Json(build_summary_response(stats, account_balances)))
}

async fn get_user_monthly_summary(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(q): Query<UserMonthlySummaryQuery>,
) -> ApiResult<Json<UserMonthlySummaryResponse>> {
    let user_id = UserId::from(claims.sub);
    let per_account_limit = q.per_account_limit.unwrap_or(500);

    // 1. Compute current-month UTC bounds (route-level shaping)
    use chrono::{Datelike, Duration, TimeZone};

    let now = Utc::now();
    let current_year = now.year();
    let current_month = now.month();

    let month_start = Utc
        .with_ymd_and_hms(current_year, current_month, 1, 0, 0, 0)
        .unwrap();
    let (next_year, next_month) = if current_month == 12 {
        (current_year + 1, 1)
    } else {
        (current_year, current_month + 1)
    };
    let next_month_start = Utc
        .with_ymd_and_hms(next_year, next_month, 1, 0, 0, 0)
        .unwrap();
    let month_end = next_month_start - Duration::seconds(1);

    // 2. Delegate to service
    let stats = state
        .tx_svc
        .compute_user_statistics(user_id, per_account_limit, Some(month_start), Some(month_end))
        .await
        .map_err(ApiError::from)?;

    // 3. Build daily cumulative spending series
    let mut daily_points: Vec<(NaiveDate, i64)> = stats
        .per_day_totals
        .into_iter()
        .filter(|(date, _)| date.year() == current_year && date.month() == current_month)
        .collect();

    daily_points.sort_by_key(|(d, _)| *d);

    let mut cumulative = 0;
    let mut daily_cumulative = Vec::with_capacity(daily_points.len());

    for (date, value) in daily_points {
        let spending = value.min(0).abs();
        cumulative += spending;
        daily_cumulative.push(DailyCumulativeSpendingPoint {
            date: date.to_string(),
            spending_cents: spending,
            cumulative_spending_cents: cumulative,
        });
    }

    let per_type_totals = stats
        .per_type_totals
        .into_iter()
        .map(|(t, v)| PerTypeTotalResponse {
            transaction_type: t.as_str().to_string(),
            total_cents: v,
        })
        .collect();

    Ok(Json(UserMonthlySummaryResponse {
        total_incoming_cents: stats.total_incoming_cents,
        total_outgoing_cents: stats.total_outgoing_cents,
        total_volume_cents: stats.total_volume_cents,
        per_type_totals,
        daily_cumulative_spending: daily_cumulative,
    }))
}

