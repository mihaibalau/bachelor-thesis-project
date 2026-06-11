use std::sync::Arc;

use axum::{extract::{Path, State}, middleware, routing::{get, post}, Extension, Json, Router};
use serde::{Deserialize, Serialize};
use crate::{
    server::{
        state::AppState,
        error::{ApiError, ApiResult},
    },
    domain::ids::UserId,
    service::{
        errors::ServiceError,
        user::{RegisterUserCommand, UserWithAccounts},
    },
};
use crate::server::auth::require_auth;
use crate::service::auth::{Claims, LoginUserCommand};

#[derive(Deserialize)]
pub struct LoginRequest {
    pub email: String,
    pub password: String,
}

#[derive(Deserialize)]
pub struct RegisterUserRequest {
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<String>,
    pub password: String,
}

#[derive(Serialize)]
pub struct LoginResponse {
    pub token: String,
    pub user_id: i64,
}

#[derive(Serialize)]
pub struct RegisterUserResponse {
    pub user_id: i64,
}

#[derive(Serialize)]
pub struct UserResponse {
    pub id: i64,
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<String>,
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
pub struct UserWithAccountsResponse {
    pub user: UserResponse,
    pub accounts: Vec<AccountResponse>,
}

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    let public = Router::new()
        .route("/",       post(register_user))
        .route("/login",  post(login_user));

    let private = Router::new()
        .route("/{id}", get(get_user_with_accounts))
        .route_layer(middleware::from_fn_with_state(
            state.clone(),
            require_auth,
        ));

    Router::new()
        .merge(public)
        .merge(private)
}

async fn login_user(
    State(state): State<Arc<AppState>>,
    Json(body): Json<LoginRequest>,
) -> ApiResult<Json<LoginResponse>> {
    // Delegate to service
    let cmd = LoginUserCommand {
        email: body.email,
        password: body.password,
    };

    let result = state.user_svc
        .login_user(cmd, &state.jwt_secret)
        .await
        .map_err(ApiError::from)?;

    Ok(Json(LoginResponse {
        token: result.token,
        user_id: result.user_id,
    }))
}

async fn register_user(
    State(state): State<Arc<AppState>>,
    Json(body): Json<RegisterUserRequest>,
) -> ApiResult<Json<RegisterUserResponse>> {
    // 1. Parse birth_date, build command
    let birth_date = body
        .birth_date
        .as_deref()
        .map(|s| chrono::NaiveDate::parse_from_str(s, "%Y-%m-%d"))
        .transpose()
        .map_err(|e| ApiError(ServiceError::Validation(e.to_string())))?;

    let cmd = RegisterUserCommand {
        tag: body.tag,
        email: body.email,
        first_name: body.first_name,
        last_name: body.last_name,
        phone: body.phone,
        birth_date,
        password: body.password,
    };

    // 2. Delegate to service
    let id = state.user_svc.register_user(cmd).await.map_err(ApiError::from)?;

    Ok(Json(RegisterUserResponse { user_id: id.into() }))
}

pub async fn get_user_with_accounts(
    State(state): State<Arc<AppState>>,
    Path(id): Path<i64>,
    Extension(claims): Extension<Claims>,
) -> ApiResult<Json<UserWithAccountsResponse>> {
    // 1. Auth check — user can only read own profile
    if claims.sub != id {
        return Err(ApiError(ServiceError::Forbidden));
    }

    let user_id = UserId::from(id);

    // 2. Delegate to service
    let UserWithAccounts { user, accounts } = state
        .user_svc
        .get_user_with_accounts(user_id)
        .await
        .map_err(ApiError::from)?;

    // 3. Map domain → response DTO
    Ok(Json(UserWithAccountsResponse {
        user: UserResponse {
            id: user.id().unwrap().0,
            tag: user.tag().to_string(),
            email: user.email().to_string(),
            first_name: user.first_name().to_string(),
            last_name: user.last_name().to_string(),
            phone: user.phone().map(|s| s.to_string()),
            birth_date: user.birth_date().map(|d| d.to_string()),
        },
        accounts: accounts
            .into_iter()
            .map(|a| AccountResponse {
                id: a.id().unwrap().0,
                account_type: a.account_type().as_str().to_string(),
                currency: a.currency().as_str().to_string(),
                balance_cents: a.balance_cents(),
                iban: a.iban().to_string(),
            })
            .collect(),
    }))
}

// async fn delete_user(
//     State(state): State<Arc<AppState>>,
//     Path(id): Path<i64>,
// ) -> ApiResult<()> {
//     let user_id = UserId::from(id);
//     state.user_svc.delete_user(user_id).await.map_err(ApiError::from)
// }