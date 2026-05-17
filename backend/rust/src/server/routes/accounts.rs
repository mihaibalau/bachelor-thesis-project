use std::collections::HashSet;
use std::str::FromStr;
use serde::{Deserialize, Serialize};
use std::sync::Arc;

#[derive(Deserialize)]
pub struct OpenAccountRequest {
    pub account_type: String,
    pub currency: String,
    pub initial_balance_cents: i64,
}

#[derive(Serialize)]
pub struct AccountResponse {
    pub id: i64,
    pub account_type: String,
    pub currency: String,
    pub balance_cents: i64,
    pub iban: String,
}

#[derive(Serialize)]
pub struct CurrencyAvailability {
    pub currency: String,
    pub available: bool,
}

#[derive(Serialize)]
pub struct AccountTypeAvailability {
    pub account_type: String,
    pub has_any_available: bool,
    pub currencies: Vec<CurrencyAvailability>,
}

#[derive(Serialize)]
pub struct AccountAvailabilityResponse {
    pub types: Vec<AccountTypeAvailability>,
}

use axum::{
    extract::{Path, State},
    middleware,
    routing::{get, post},
    Extension, Json, Router,
};
use tracing::info;

use crate::{
    domain::{
        ids::{AccountId, UserId},
        value::{
            account_type::AccountType,
            currency::Currency,
        },
    },
    server::{
        auth::require_auth,
        error::{ApiError, ApiResult},
        state::AppState,
    },
    service::{
        auth::Claims,
        errors::ServiceError,
    },
};

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    let private = Router::new()
        .route("/", post(open_account))
        .route("/{id}", get(get_account))
        .route("/availability", get(get_availability))
        .route_layer(middleware::from_fn_with_state(
            state.clone(),
            require_auth,
        ));

    Router::new().merge(private)
}

async fn open_account(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<OpenAccountRequest>,
) -> ApiResult<Json<AccountResponse>> {
    info!(method = "POST", path = "/api/accounts", "incoming request");

    // 1. Check the account type
    let account_type = match body.account_type.as_str() {
        "Savings" => AccountType::Savings,
        "Credit"  => AccountType::Credit,
        "Regular" => AccountType::Regular,
        _ => {
            return Err(ApiError(ServiceError::Validation(
                "invalid account_type".to_string(),
            )))
        }
    };

    // 2. Check the currency
    let currency = Currency::from_str(&body.currency).map_err(|_| {
        ApiError(ServiceError::Validation("invalid currency".to_string()))
    })?;

    // 3. Auth user is the owner
    let user_id = UserId::from(claims.sub);

    // 4. Open the account
    let account_id = state
        .account_svc
        .open_account(
            user_id,
            account_type,
            currency,
            body.initial_balance_cents,
        )
        .await
        .map_err(ApiError::from)?;

    // 5. Get the created account
    let account = state
        .account_svc
        .get_account(account_id)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(AccountResponse {
        id: account.id().unwrap().0,
        account_type: account.account_type().as_str().to_string(),
        currency: account.currency().as_str().to_string(),
        balance_cents: account.balance_cents(),
        iban: account.iban().to_string(),
    }))
}

async fn get_account(
    State(state): State<Arc<AppState>>,
    Path(id): Path<i64>,
    Extension(claims): Extension<Claims>,
) -> ApiResult<Json<AccountResponse>> {
    info!(method = "GET", path = "/api/accounts/{id}", id, "incoming request");

    let account_id = AccountId::from(id);

    let account = state
        .account_svc
        .get_account(account_id)
        .await
        .map_err(ApiError::from)?;

    if account.user_id().0 != claims.sub {
        return Err(ApiError(ServiceError::Forbidden));
    }

    Ok(Json(AccountResponse {
        id: account.id().unwrap().0,
        account_type: account.account_type().as_str().to_string(),
        currency: account.currency().as_str().to_string(),
        balance_cents: account.balance_cents(),
        iban: account.iban().to_string(),
    }))
}

async fn get_availability(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
) -> ApiResult<Json<AccountAvailabilityResponse>> {
    info!(method = "GET", path = "/api/accounts/availability", "incoming request");

    let user_id = UserId::from(claims.sub);

    let availability = state
        .account_svc
        .get_account_availability(user_id)
        .await
        .map_err(ApiError::from)?;


    let types = AccountType::all()
        .iter()
        .map(|&account_type| {
            let free = availability.available.get(&account_type).cloned().unwrap_or_default();
            let free_set: HashSet<_> = free.iter().copied().collect();

            let currencies = Currency::all()
                .iter()
                .map(|&c| CurrencyAvailability {
                    currency: c.as_str().to_string(),
                    available: free_set.contains(&c),
                })
                .collect();

            AccountTypeAvailability {
                account_type: account_type.as_str().to_string(),
                has_any_available: !free.is_empty(),
                currencies,
            }
        })
        .collect();

    Ok(Json(AccountAvailabilityResponse { types }))
}