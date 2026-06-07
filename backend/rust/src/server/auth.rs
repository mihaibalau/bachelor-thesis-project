use axum::{
    extract::{Request, State},
    http::StatusCode,
    middleware::Next,
    response::{IntoResponse, Response},
    Json,
};
use jsonwebtoken::{decode, DecodingKey, Validation};
use serde::Serialize;
use std::sync::Arc;
use crate::{server::state::AppState, service::auth::Claims};

#[derive(Serialize)]
struct UnauthorizedBody {
    status: u16,
    code: &'static str,
    message: &'static str,
}

fn unauthorized_response() -> Response {
    (
        StatusCode::UNAUTHORIZED,
        Json(UnauthorizedBody {
            status: 401,
            code: "unauthorized",
            message: "missing or invalid token",
        }),
    )
        .into_response()
}

pub async fn require_auth(
    State(state): State<Arc<AppState>>,
    mut req: Request,
    next: Next,
) -> Result<Response, Response> {
    // 1. Extract Bearer token
    let token = req
        .headers()
        .get("Authorization")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "))
        .ok_or_else(unauthorized_response)?;

    // 2. Decode JWT and attach claims
    let claims = decode::<Claims>(
        token,
        &DecodingKey::from_secret(state.jwt_secret.as_bytes()),
        &Validation::default(),
    )
    .map_err(|e| {
        tracing::debug!(error = %e, "jwt validation failed");
        unauthorized_response()
    })?
    .claims;

    req.extensions_mut().insert(claims);

    // 3. Continue to handler
    Ok(next.run(req).await)
}
