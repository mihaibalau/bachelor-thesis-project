use std::collections::{BTreeMap, HashMap, HashSet};
use std::sync::Arc;

use axum::{
    extract::State,
    middleware,
    routing::get,
    Extension, Json, Router,
};
use chrono::{Datelike, Duration, NaiveDate, TimeZone, Utc, Weekday};
use serde::Serialize;
use crate::{
    domain::{
        ids::{AccountId, UserId},
        transaction::Transaction,
        value::transaction_type::TransactionType,
    },
    server::{
        auth::require_auth,
        error::{ApiError, ApiResult},
        state::AppState,
    },
    service::{
        affiliate::ListAffiliatesParams,
        auth::Claims,
        errors::ServiceError,
        user::UserWithAccounts,
    },
};

#[derive(Serialize)]
pub struct DashboardActivityItem {
    pub id: i64,
    pub label: String,
    pub description: String,
    pub recorded_on: String,
    pub amount_cents: i64,
    pub category: String,
    pub is_income: bool,
}

#[derive(Serialize)]
pub struct DashboardDailySpendingPoint {
    pub day_label: String,
    pub date: String,
    pub cumulative_spending_cents: i64,
}

#[derive(Serialize)]
pub struct DashboardSpending {
    pub total_spent_cents: i64,
    pub change_percent_vs_last_month: f64,
    pub daily_cumulative: Vec<DashboardDailySpendingPoint>,
}

#[derive(Serialize)]
pub struct DashboardResponse {
    pub total_balance_cents: i64,
    pub balance_change_percent: f64,
    pub active_accounts_count: u32,
    pub affiliates_count: u64,
    pub transfers_total_cents: i64,
    pub recent_activity: Vec<DashboardActivityItem>,
    pub spending: DashboardSpending,
}

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    Router::new()
        .route("/", get(get_dashboard))
        .route_layer(middleware::from_fn_with_state(state, require_auth))
}

fn month_bounds(year: i32, month: u32) -> (chrono::DateTime<Utc>, chrono::DateTime<Utc>) {
    let start = Utc
        .with_ymd_and_hms(year, month, 1, 0, 0, 0)
        .unwrap();
    let (next_year, next_month) = if month == 12 {
        (year + 1, 1)
    } else {
        (year, month + 1)
    };
    let next_start = Utc
        .with_ymd_and_hms(next_year, next_month, 1, 0, 0, 0)
        .unwrap();
    let end = next_start - Duration::seconds(1);
    (start, end)
}

fn percent_change(current: i64, previous: i64) -> f64 {
    if previous == 0 {
        if current == 0 {
            0.0
        } else {
            100.0
        }
    } else {
        ((current - previous) as f64 / previous as f64) * 100.0
    }
}

fn weekday_short(date: NaiveDate) -> &'static str {
    match date.weekday() {
        Weekday::Mon => "Mon",
        Weekday::Tue => "Tue",
        Weekday::Wed => "Wed",
        Weekday::Thu => "Thu",
        Weekday::Fri => "Fri",
        Weekday::Sat => "Sat",
        Weekday::Sun => "Sun",
    }
}

fn chart_day_label(date: NaiveDate) -> String {
    format!("{} {}", weekday_short(date), date.day())
}

fn parse_payment_fields(description: &str) -> (String, String) {
    let mut category = String::from("shopping");
    let mut merchant = String::from("Payment");

    for part in description.split('|').map(str::trim) {
        if let Some(value) = part.strip_prefix("category:") {
            category = value.trim().to_string();
        } else if let Some(value) = part.strip_prefix("merchant:") {
            merchant = value.trim().to_string();
        }
    }

    (merchant, category)
}

fn ui_category(tx_type: &TransactionType, payment_category: &str, description: &str) -> String {
    match tx_type {
        TransactionType::Withdrawal => "atm".to_string(),
        TransactionType::Send | TransactionType::Transfer => "transfer".to_string(),
        TransactionType::Deposit => "salary".to_string(),
        TransactionType::Payment => {
            let lower = payment_category.to_lowercase();
            if lower.contains("food")
                || lower.contains("dinner")
                || lower.contains("restaurant")
            {
                "food".to_string()
            } else if description.to_lowercase().contains("glovo") {
                "food".to_string()
            } else {
                "shopping".to_string()
            }
        }
    }
}

fn map_activity_item(
    tx: &Transaction,
    user_account_ids: &HashSet<AccountId>,
) -> Result<DashboardActivityItem, ServiceError> {
    let id = tx
        .id()
        .ok_or_else(|| ServiceError::internal("transaction missing id"))?
        .0;

    let from_owned = user_account_ids.contains(&tx.from_account_id());
    let to_owned = user_account_ids.contains(&tx.to_account_id());

    let amount_cents = if to_owned && !from_owned {
        tx.value_cents()
    } else if from_owned && !to_owned {
        -tx.value_cents()
    } else if from_owned && to_owned {
        -tx.value_cents()
    } else {
        0
    };

    let description = tx.description();
    let (label, detail, category) = match tx.transaction_type() {
        TransactionType::Payment => {
            let (merchant, payment_category) = parse_payment_fields(description);
            let note = description
                .split('|')
                .find_map(|part| part.trim().strip_prefix("note:"))
                .map(str::trim)
                .filter(|n| !n.is_empty());
            let detail = note
                .map(|n| n.to_string())
                .unwrap_or_else(|| payment_category.clone());
            (merchant, detail, ui_category(tx.transaction_type(), &payment_category, description))
        }
        TransactionType::Withdrawal => (
            "ATM Withdrawal".to_string(),
            "Cash out".to_string(),
            ui_category(tx.transaction_type(), "", description),
        ),
        TransactionType::Send => {
            let raw = description
                .strip_prefix("Send:")
                .map(str::trim)
                .unwrap_or("Transfer");
            let recipient = raw.split('|').next().unwrap_or(raw).trim();
            let note = raw
                .split('|')
                .find_map(|part| part.trim().strip_prefix("note:"))
                .map(str::trim)
                .filter(|n| !n.is_empty());
            (
                format!("Transfer – {}", recipient),
                note.map(|n| n.to_string())
                    .unwrap_or_else(|| "Rent share".to_string()),
                ui_category(tx.transaction_type(), "", description),
            )
        }
        TransactionType::Transfer => (
            "Transfer".to_string(),
            description.to_string(),
            ui_category(tx.transaction_type(), "", description),
        ),
        TransactionType::Deposit => {
            if description.to_lowercase().contains("salary") {
                (
                    "Salary".to_string(),
                    "Gentlix Bank".to_string(),
                    "salary".to_string(),
                )
            } else if description.contains("Opening balance") {
                (
                    "Balance adjustment".to_string(),
                    description.to_string(),
                    "salary".to_string(),
                )
            } else {
                (
                    "Deposit".to_string(),
                    description.to_string(),
                    "salary".to_string(),
                )
            }
        }
    };

    Ok(DashboardActivityItem {
        id,
        label,
        description: detail,
        recorded_on: tx.recorded_on().to_rfc3339(),
        amount_cents,
        category,
        is_income: amount_cents > 0,
    })
}

fn transfers_total(per_type_totals: &HashMap<TransactionType, i64>) -> i64 {
    per_type_totals
        .get(&TransactionType::Send)
        .copied()
        .unwrap_or(0)
        + per_type_totals
            .get(&TransactionType::Transfer)
            .copied()
            .unwrap_or(0)
}

fn spending_outgoing(per_type_totals: &HashMap<TransactionType, i64>) -> i64 {
    per_type_totals
        .get(&TransactionType::Payment)
        .copied()
        .unwrap_or(0)
        + per_type_totals
            .get(&TransactionType::Withdrawal)
            .copied()
            .unwrap_or(0)
}

fn build_spending_daily_cumulative(
    transactions: &[Transaction],
    user_account_ids: &HashSet<AccountId>,
    year: i32,
    month: u32,
    from: chrono::DateTime<Utc>,
    to: chrono::DateTime<Utc>,
) -> (Vec<DashboardDailySpendingPoint>, i64) {
    let mut per_day: BTreeMap<NaiveDate, i64> = BTreeMap::new();

    for tx in transactions {
        let recorded = tx.recorded_on();
        if recorded < from || recorded > to {
            continue;
        }

        let tx_type = *tx.transaction_type();
        if tx_type != TransactionType::Payment && tx_type != TransactionType::Withdrawal {
            continue;
        }

        if !user_account_ids.contains(&tx.from_account_id()) {
            continue;
        }

        *per_day.entry(recorded.date_naive()).or_insert(0) += tx.value_cents();
    }

    let month_start = NaiveDate::from_ymd_opt(year, month, 1).unwrap();
    let month_end = if month == 12 {
        NaiveDate::from_ymd_opt(year + 1, 1, 1)
            .unwrap()
            .pred_opt()
            .unwrap()
    } else {
        NaiveDate::from_ymd_opt(year, month + 1, 1)
            .unwrap()
            .pred_opt()
            .unwrap()
    };

    let today = Utc::now().date_naive();
    let plot_end = month_end.min(today);

    let mut cumulative = 0i64;
    let mut points = Vec::new();
    let mut cursor = month_start;

    while cursor <= plot_end {
        if let Some(spending) = per_day.get(&cursor) {
            cumulative += spending;
        }

        points.push(DashboardDailySpendingPoint {
            day_label: chart_day_label(cursor),
            date: cursor.to_string(),
            cumulative_spending_cents: cumulative,
        });

        cursor = cursor.succ_opt().unwrap();
    }

    (points, cumulative)
}

async fn collect_user_transactions(
    state: &Arc<AppState>,
    user_id: UserId,
    accounts: &[crate::domain::account::Account],
    per_account_limit: i64,
) -> Result<Vec<Transaction>, ApiError> {
    let mut unique: HashMap<crate::domain::ids::TransactionId, Transaction> = HashMap::new();

    for account in accounts {
        let account_id = account
            .id()
            .ok_or_else(|| ApiError(ServiceError::internal("account missing id")))?;
        let txs = state
            .tx_svc
            .list_recent_for_user(user_id, account_id, Some(per_account_limit), None)
            .await
            .map_err(ApiError::from)?;
        for tx in txs {
            if let Some(id) = tx.id() {
                unique.entry(id).or_insert(tx);
            }
        }
    }

    Ok(unique.into_values().collect())
}

async fn get_dashboard(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
) -> ApiResult<Json<DashboardResponse>> {
    let user_id = UserId::from(claims.sub);
    let per_account_limit = 500i64;

    // 1. Load user accounts and balances
    let UserWithAccounts { accounts, .. } = state
        .user_svc
        .get_user_with_accounts(user_id)
        .await
        .map_err(ApiError::from)?;

    let total_balance_cents: i64 = accounts.iter().map(|a| a.balance_cents()).sum();
    let active_accounts_count = accounts.len() as u32;

    // 2. Affiliate count (total only)
    let affiliates = state
        .affiliate_svc
        .list_affiliates_view(
            user_id,
            ListAffiliatesParams {
                page: Some(1),
                page_size: Some(1),
                search: None,
                currency: None,
                for_send_currency: None,
                sort: None,
            },
        )
        .await
        .map_err(ApiError::from)?;

    // 3. Current and previous month stats
    let now = Utc::now();
    let current_year = now.year();
    let current_month = now.month();
    let (month_start, month_end) = month_bounds(current_year, current_month);

    let prev_month = if current_month == 1 { 12 } else { current_month - 1 };
    let prev_year = if current_month == 1 {
        current_year - 1
    } else {
        current_year
    };
    let (prev_start, prev_end) = month_bounds(prev_year, prev_month);

    let current_stats = state
        .tx_svc
        .compute_user_statistics(user_id, per_account_limit, Some(month_start), Some(month_end))
        .await
        .map_err(ApiError::from)?;

    let prev_stats = state
        .tx_svc
        .compute_user_statistics(user_id, per_account_limit, Some(prev_start), Some(prev_end))
        .await
        .map_err(ApiError::from)?;

    let net_change =
        current_stats.total_incoming_cents - current_stats.total_outgoing_cents;
    let balance_at_month_start = total_balance_cents - net_change;
    let balance_change_percent = percent_change(net_change, balance_at_month_start);

    let transfers_total_cents = transfers_total(&current_stats.per_type_totals);

    // 4. Spending chart and recent activity
    let user_account_ids: HashSet<AccountId> =
        accounts.iter().filter_map(|a| a.id()).collect();

    let all_transactions =
        collect_user_transactions(&state, user_id, &accounts, per_account_limit).await?;

    let (daily_cumulative, total_spent_cents) = build_spending_daily_cumulative(
        &all_transactions,
        &user_account_ids,
        current_year,
        current_month,
        month_start,
        month_end,
    );

    let current_spending = spending_outgoing(&current_stats.per_type_totals);
    let prev_spending = spending_outgoing(&prev_stats.per_type_totals);
    let spending_change_percent = percent_change(current_spending, prev_spending);

    const RECENT_ACTIVITY_LIMIT: usize = 5;

    let mut recent: Vec<Transaction> = all_transactions;
    recent.sort_by(|a, b| b.recorded_on().cmp(&a.recorded_on()));

    let mut recent_activity = Vec::new();
    for tx in recent.into_iter().take(RECENT_ACTIVITY_LIMIT) {
        recent_activity.push(map_activity_item(&tx, &user_account_ids).map_err(ApiError::from)?);
    }

    Ok(Json(DashboardResponse {
        total_balance_cents,
        balance_change_percent,
        active_accounts_count,
        affiliates_count: affiliates.total,
        transfers_total_cents,
        recent_activity,
        spending: DashboardSpending {
            total_spent_cents,
            change_percent_vs_last_month: spending_change_percent,
            daily_cumulative,
        },
    }))
}
