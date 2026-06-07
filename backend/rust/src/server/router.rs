use std::sync::Arc;
use axum::http::HeaderValue;
use axum::Router;
use tower_http::cors::{AllowOrigin, Any, CorsLayer};

use crate::server::{logging, routes, state::AppState};

pub fn create_router(state: Arc<AppState>) -> Router {
    // 1. CORS layer from env
    let cors_origin = std::env::var("CORS_ORIGIN")
        .unwrap_or_else(|_| "http://localhost:5173".to_string());
    let allow_origin = AllowOrigin::exact(
        HeaderValue::from_str(&cors_origin).expect("invalid CORS_ORIGIN"),
    );
    let cors = CorsLayer::new()
        .allow_origin(allow_origin)
        .allow_methods(Any)
        .allow_headers(Any);

    // 2. Nest API route modules, trace layer, shared state
    Router::new()
        .nest("/api/users",        routes::users::router(state.clone()))
        .nest("/api/accounts",     routes::accounts::router(state.clone()))
        .nest("/api/affiliates",   routes::affiliates::router(state.clone()))
        .nest("/api/transactions", routes::transactions::router(state.clone()))
        .nest("/api/dashboard",    routes::dashboard::router(state.clone()))
        .layer(cors)
        .layer(logging::http_trace_layer())
        .with_state(state)
}
