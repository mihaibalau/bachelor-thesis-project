use std::sync::Arc;
use axum::Router;
use tower_http::{cors::CorsLayer, trace::TraceLayer};

use crate::{
    server::{
        state::AppState,
        routes,
    },
};

pub fn create_router(state: Arc<AppState>) -> Router {
    Router::new()
        .nest("/api/users",        routes::users::router())
        // .nest("/api/accounts",     routes::accounts::router())
        // .nest("/api/transactions", routes::transactions::router())
        // .nest("/api/affiliates",   routes::affiliates::router())
        .layer(CorsLayer::permissive())
        .layer(TraceLayer::new_for_http())
        .with_state(state)
}