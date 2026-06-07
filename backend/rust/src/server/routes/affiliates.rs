use std::sync::Arc;

use axum::{
    extract::{Path, Query, State},
    middleware,
    routing::{get, post},
    Extension, Json, Router,
};
use serde::{Deserialize, Serialize};

use crate::{
    domain::ids::{AccountId, UserId},
    server::{
        auth::require_auth,
        error::{ApiError, ApiResult},
        state::AppState,
    },
    service::{
        auth::Claims,
        errors::ServiceError,
        affiliate::ListAffiliatesParams,
    },
};

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
    pub for_send_currency: Option<String>,
    pub sort: Option<String>, // "asc" | "desc"
}

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
    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(body.recipient_sub_account_id);

    // 1. Delegate to service
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
    let owner_id = UserId::from(claims.sub);
    let recipient_sub_account_id = AccountId::from(sub_account_id);

    let view = state
        .affiliate_svc
        .get_affiliate_view(owner_id, recipient_sub_account_id)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(AffiliateResponse {
        recipient_sub_account_id: view.recipient_sub_account_id,
        nickname: view.nickname,
        recipient_full_name: view.recipient_full_name,
        currency: view.currency,
    }))
}

async fn list_affiliates(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Query(query): Query<ListAffiliatesQuery>,
) -> ApiResult<Json<PaginatedAffiliatesResponse>> {
    let owner_id = UserId::from(claims.sub);

    // 1. Map query → service params, delegate
    let params = ListAffiliatesParams {
        page: query.page,
        page_size: query.page_size,
        search: query.search,
        currency: query.currency,
        for_send_currency: query.for_send_currency,
        sort: query.sort,
    };

    let view = state
        .affiliate_svc
        .list_affiliates_view(owner_id, params)
        .await
        .map_err(ApiError::from)?;

    // 2. Map view → response DTO
    let items: Vec<AffiliateResponse> = view
        .items
        .into_iter()
        .map(|v| AffiliateResponse {
            recipient_sub_account_id: v.recipient_sub_account_id,
            nickname: v.nickname,
            recipient_full_name: v.recipient_full_name,
            currency: v.currency,
        })
        .collect();

    Ok(Json(PaginatedAffiliatesResponse {
        items,
        page: view.page,
        page_size: view.page_size,
        total: view.total,
    }))
}

async fn resolve_affiliate_target(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
    Json(body): Json<ResolveAffiliateTargetRequest>,
) -> ApiResult<Json<ResolveAffiliateTargetResponse>> {
    let owner_id = UserId::from(claims.sub);

    // 1. Resolve by identifier type
    let view = match body.identifier_type {
        IdentifierType::Tag => state
            .affiliate_svc
            .resolve_target_by_tag(owner_id, body.identifier)
            .await
            .map_err(ApiError::from)?,
        IdentifierType::Phone => {
            return Err(ApiError(ServiceError::Validation(
                "phone identifier not supported yet".into(),
            )));
        }
    };

    let currencies = view
        .currencies
        .into_iter()
        .map(|c| ResolveAffiliateCurrencyOption {
            currency: c.currency,
            recipient_sub_account_id: c.recipient_sub_account_id,
        })
        .collect();

    Ok(Json(ResolveAffiliateTargetResponse {
        recipient_user_id: view.recipient_user_id,
        recipient_full_name: view.recipient_full_name,
        currencies,
    }))
}