use std::sync::Arc;

use axum::{
    extract::State,
    middleware,
    routing::get,
    Extension, Json, Router,
};

use crate::server::{
    auth::require_auth,
    error::{ApiError, ApiResult},
    state::AppState,
};
use crate::{
    domain::ids::UserId,
    service::{auth::Claims, dashboard::DashboardData},
};

pub fn router(state: Arc<AppState>) -> Router<Arc<AppState>> {
    Router::new()
        .route("/", get(get_dashboard))
        .route_layer(middleware::from_fn_with_state(state, require_auth))
}

async fn get_dashboard(
    State(state): State<Arc<AppState>>,
    Extension(claims): Extension<Claims>,
) -> ApiResult<Json<DashboardData>> {
    let user_id = UserId::from(claims.sub);
    let data = state
        .dashboard_svc
        .get_dashboard(user_id)
        .await
        .map_err(ApiError::from)?;
    Ok(Json(data))
}
