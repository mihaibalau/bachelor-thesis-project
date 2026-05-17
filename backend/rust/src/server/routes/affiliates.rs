use std::sync::Arc;
use std::collections::HashSet;

use axum::{
    extract::{Path, Query, State},
    middleware,
    routing::{delete, get, patch, post},
    Extension, Json, Router,
};
use serde::{Deserialize, Serialize};
use tracing::info;

use crate::{
    domain::{
        ids::{AccountId, UserId},
        value::currency::Currency,
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

// ── Request DTOs ─────────────────────────────────────────────────────────────

#[derive(Deserialize)]
pub struct CreateAffiliateRequest {
    pub recipient_sub_account_id: i64,
    pub nickname: String,
}

#[derive(Deserialize)]
pub struct UpdateAffiliateNicknameRequest {
    pub nickname: String,
}

#[derive(Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum IdentifierType {
    Tag,
    Phone,
}

#[derive(Deserialize)]
pub struct ResolveAffiliateTargetRequest {
    pub identifier_type: IdentifierType,
    pub identifier: String,
}

#[derive(Deserialize)]
pub struct ListAffiliatesQuery {
    pub page: Option<u32>,
    pub page_size: Option<u32>,
    pub search: Option<String>,
    pub currency: Option<String>,
    pub sort: Option<String>, // "asc" | "desc"
}

// ── Response DTOs ────────────────────────────────────────────────────────────

#[derive(Serialize)]
pub struct AffiliateResponse {
    pub recipient_sub_account_id: i64,
    pub nickname: String,
    pub recipient_full_name: String,
    pub currency: String,
}

#[derive(Serialize)]
pub struct PaginatedAffiliatesResponse {
    pub items: Vec<AffiliateResponse>,
    pub page: u32,
    pub page_size: u32,
    pub total: u64,
}

#[derive(Serialize)]
pub struct ResolveAffiliateCurrencyOption {
    pub currency: String,
    pub recipient_sub_account_id: i64,
}

#[derive(Serialize)]
pub struct ResolveAffiliateTargetResponse {
    pub recipient_user_id: i64,
    pub recipient_full_name: String,
    pub currencies: Vec<ResolveAffiliateCurrencyOption>,
}

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    let private = Router::new()

        .route("/", get(list_affiliates).post(create_affiliate))
        .route(
            "/{sub_account_id}",
            get(get_affiliate)
                .patch(update_affiliate_nickname)
                .delete(delete_affiliate),
        )

        .route("/resolve-target", post(resolve_affiliate_target))
        .route_layer(middleware::from_fn_with_state(
            state.clone(),
            require_auth,
        ));

    Router::new().merge(private)
}

async fn create_affiliate(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<CreateAffiliateRequest>,
) -> ApiResult<()> {
    info!(method = "POST", path = "/api/affiliates", "incoming request");

    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(body.recipient_sub_account_id);

    state
        .affiliate_svc
        .create_affiliate(owner_id, recipient_sub_account_id, body.nickname)
        .await
        .map_err(ApiError::from)
}

async fn update_affiliate_nickname(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Path(sub_account_id): Path<i64>,
    Json(body): Json<UpdateAffiliateNicknameRequest>,
) -> ApiResult<()> {
    info!(
        method = "PATCH",
        path = "/api/affiliates/{sub_account_id}",
        sub_account_id,
        "incoming request"
    );

    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(sub_account_id);

    state
        .affiliate_svc
        .rename_affiliate(owner_id, recipient_sub_account_id, body.nickname)
        .await
        .map_err(ApiError::from)
}

async fn delete_affiliate(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Path(sub_account_id): Path<i64>,
) -> ApiResult<()> {
    info!(
        method = "DELETE",
        path = "/api/affiliates/{sub_account_id}",
        sub_account_id,
        "incoming request"
    );

    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(sub_account_id);

    state
        .affiliate_svc
        .delete_affiliate(owner_id, recipient_sub_account_id)
        .await
        .map_err(ApiError::from)
}

async fn get_affiliate(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Path(sub_account_id): Path<i64>,
) -> ApiResult<Json<AffiliateResponse>> {
    info!(
        method = "GET",
        path = "/api/affiliates/{sub_account_id}",
        sub_account_id,
        "incoming request"
    );

    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(sub_account_id);

    let affiliate = state
        .affiliate_svc
        .get(owner_id, recipient_sub_account_id)
        .await
        .map_err(ApiError::from)?;

    let account = state
        .account_svc
        .get_account(affiliate.recipient_sub_account_id())
        .await
        .map_err(ApiError::from)?;

    let recipient_user_id = account.user_id();
    let recipient_user = state
        .user_svc
        .get_user(recipient_user_id)
        .await
        .map_err(ApiError::from)?;

    let full_name = format!(
        "{} {}",
        recipient_user.first_name(),
        recipient_user.last_name()
    );

    Ok(Json(AffiliateResponse {
        recipient_sub_account_id: affiliate.recipient_sub_account_id().0,
        nickname: affiliate.nickname().to_string(),
        recipient_full_name: full_name,
        currency: account.currency().as_str().to_string(),
    }))
}

async fn list_affiliates(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(query): Query<ListAffiliatesQuery>,
) -> ApiResult<Json<PaginatedAffiliatesResponse>> {
    info!(method = "GET", path = "/api/affiliates", "incoming request");

    let owner_id = UserId::from(claims.sub);

    let affiliates = state
        .affiliate_svc
        .list_for_owner(owner_id)
        .await
        .map_err(ApiError::from)?;

    let mut items = Vec::with_capacity(affiliates.len());

    for a in affiliates {
        let account = state
            .account_svc
            .get_account(a.recipient_sub_account_id())
            .await
            .map_err(ApiError::from)?;

        let user = state
            .user_svc
            .get_user(account.user_id())
            .await
            .map_err(ApiError::from)?;

        let full_name = format!("{} {}", user.first_name(), user.last_name());

        items.push(AffiliateResponse {
            recipient_sub_account_id: a.recipient_sub_account_id().0,
            nickname: a.nickname().to_string(),
            recipient_full_name: full_name,
            currency: account.currency().as_str().to_string(),
        });
    }

    // Search (min 2 chars)
    let mut filtered = items;
    if let Some(ref s) = query.search {
        let s_trim = s.trim().to_lowercase();
        if s_trim.len() >= 2 {
            filtered = filtered
                .into_iter()
                .filter(|item| {
                    let nick = item.nickname.to_lowercase();
                    let name = item.recipient_full_name.to_lowercase();
                    nick.contains(&s_trim) || name.contains(&s_trim)
                })
                .collect();
        }
    }

    // Filter by currency
    if let Some(ref curr) = query.currency {
        let curr_up = curr.to_uppercase();
        filtered = filtered
            .into_iter()
            .filter(|item| item.currency.eq_ignore_ascii_case(&curr_up))
            .collect();
    }

    // Sort
    let sort_dir = query.sort.as_deref().unwrap_or("asc");
    filtered.sort_by(|a, b| {
        let ord = a.nickname.to_lowercase().cmp(&b.nickname.to_lowercase());
        if sort_dir == "desc" {
            ord.reverse()
        } else {
            ord
        }
    });

    // Pagination
    let page = query.page.unwrap_or(1).max(1);
    let page_size = query.page_size.unwrap_or(20).clamp(1, 100);
    let total = filtered.len() as u64;

    let start = ((page - 1) * page_size) as usize;
    let end = (start + page_size as usize).min(filtered.len());

    let page_items: Vec<AffiliateResponse> = if start >= filtered.len() {
        Vec::new()
    } else {
        filtered
            .into_iter()
            .skip(start)
            .take(page_size as usize)
            .collect()
    };

    Ok(Json(PaginatedAffiliatesResponse {
        items: page_items,
        page,
        page_size,
        total,
    }))
}

async fn resolve_affiliate_target(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<ResolveAffiliateTargetRequest>,
) -> ApiResult<Json<ResolveAffiliateTargetResponse>> {
    info!(method = "POST", path = "/api/affiliates/resolve-target", "incoming request");

    let owner_id = UserId::from(claims.sub);
    let identifier = body.identifier.trim();

    if identifier.is_empty() {
        return Err(ApiError(ServiceError::Validation(
            "identifier cannot be empty".into(),
        )));
    }

    let target_user = state
        .user_svc
        .find_by_tag(identifier)
        .await
        .map_err(ApiError::from)?;

    let target_user_id = target_user.id().ok_or_else(|| {
        ApiError(ServiceError::Validation(
            "target user has no id in memory".to_string(),
        ))
    })?;

    let owner_accounts = state
        .account_svc
        .list_for_user(owner_id)
        .await
        .map_err(ApiError::from)?;

    let target_accounts = state
        .account_svc
        .list_for_user(target_user_id)
        .await
        .map_err(ApiError::from)?;

    let owner_currencies: HashSet<_> = owner_accounts
        .iter()
        .map(|a| a.currency())
        .collect();

    let mut options = Vec::new();

    for acc in target_accounts {

        let curr = acc.currency();
        if owner_currencies.contains(&curr) {
            options.push(ResolveAffiliateCurrencyOption {
                currency: curr.as_str().to_string(),
                recipient_sub_account_id: acc.id().unwrap().0,
            });
        }
    }

    if options.is_empty() {
        return Err(ApiError(ServiceError::Validation(
            "no compatible currencies between owner and target user".into(),
        )));
    }

    let full_name = format!("{} {}", target_user.first_name(), target_user.last_name());

    Ok(Json(ResolveAffiliateTargetResponse {
        recipient_user_id: target_user_id.0,
        recipient_full_name: full_name,
        currencies: options,
    }))
}