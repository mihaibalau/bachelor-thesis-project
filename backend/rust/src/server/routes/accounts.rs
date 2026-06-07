use std::collections::HashSet;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use axum::{
    extract::{Path, State},
    middleware,
    routing::{get, post},
    Extension, Json, Router,
};
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

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    let private = Router::new()
        .route("/", post(open_account))
        // Static /availability before /{id} so it is not captured as an id
        .route("/availability", get(get_availability))
        .route("/{id}", get(get_account))
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
    let user_id = UserId::from(claims.sub);

    // 1. Delegate to service
    let account_id = state
        .account_svc
        .open_account_raw(
            user_id,
            body.account_type,
            body.currency,
            body.initial_balance_cents,
        )
        .await
        .map_err(ApiError::from)?;

    // 2. Load created account for response
    let account = state
        .account_svc
        .get_account(account_id)
        .await
        .map_err(ApiError::from)?;

    let id = account
        .id()
        .ok_or_else(|| ApiError(ServiceError::internal("account missing id")))?
        .0;

    Ok(Json(AccountResponse {
        id,
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
    let account_id = AccountId::from(id);

    // 1. Delegate to service
    let account = state
        .account_svc
        .get_account(account_id)
        .await
        .map_err(ApiError::from)?;

    // 2. Auth check — hide other users' accounts as not found
    if account.user_id().0 != claims.sub {
        return Err(ApiError(ServiceError::not_found("account")));
    }

    let id = account
        .id()
        .ok_or_else(|| ApiError(ServiceError::internal("account missing id")))?
        .0;

    Ok(Json(AccountResponse {
        id,
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
    let user_id = UserId::from(claims.sub);

    // 1. Delegate to service
    let availability = state
        .account_svc
        .get_account_availability(user_id)
        .await
        .map_err(ApiError::from)?;

    // 2. Map to response DTO
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