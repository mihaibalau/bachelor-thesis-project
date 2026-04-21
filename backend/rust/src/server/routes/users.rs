use std::sync::Arc;

use axum::{
    extract::{Path, State},
    routing::{delete, get, post},
    Json, Router,
};
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

#[derive(Deserialize)]
pub struct RegisterUserRequest {
    pub tag: String,
    pub email: String,
    pub first_name: String,
    pub last_name: String,
    pub phone: Option<String>,
    pub birth_date: Option<String>,
    pub password_hash: String,
}

#[derive(Serialize)]
pub struct RegisterUserResponse {
    pub user_id: i64,
}

#[derive(Serialize)]
pub struct UserWithAccountsResponse {
    pub user: String,
    pub accounts: Vec<String>,
}

pub fn router() -> Router<Arc<AppState>> {
    Router::new()
        .route("/",      post(register_user))
        .route("/{id}", get(get_user_with_accounts).delete(delete_user))
}

async fn register_user(
    State(state): State<Arc<AppState>>,
    Json(body): Json<RegisterUserRequest>,
) -> ApiResult<Json<RegisterUserResponse>> {
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
        password_hash: body.password_hash,
    };

    let id = state.user_svc.register_user(cmd).await.map_err(ApiError::from)?;

    Ok(Json(RegisterUserResponse { user_id: id.into() }))
}

async fn get_user_with_accounts(
    State(state): State<Arc<AppState>>,
    Path(id): Path<i64>,
) -> ApiResult<Json<UserWithAccountsResponse>> {
    let user_id = UserId::from(id);

    let UserWithAccounts { user, accounts } =
        state.user_svc.get_user_with_accounts(user_id).await.map_err(ApiError::from)?;

    Ok(Json(UserWithAccountsResponse {
        user: format!("{:?}", user),
        accounts: accounts.into_iter().map(|a| format!("{:?}", a)).collect(),
    }))
}

async fn delete_user(
    State(state): State<Arc<AppState>>,
    Path(id): Path<i64>,
) -> ApiResult<()> {
    let user_id = UserId::from(id);
    state.user_svc.delete_user(user_id).await.map_err(ApiError::from)
}