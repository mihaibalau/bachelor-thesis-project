use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

use axum::http::{Request, Response};
use tower_http::trace::TraceLayer;
use tracing::Span;

static REQ_COUNTER: AtomicU64 = AtomicU64::new(1);

fn request_id_from_headers<B>(request: &Request<B>) -> String {
    request
        .headers()
        .get("x-request-id")
        .or_else(|| request.headers().get("x-amzn-trace-id"))
        .and_then(|v| v.to_str().ok())
        .map(str::to_string)
        .unwrap_or_else(|| {
            let n = REQ_COUNTER.fetch_add(1, Ordering::Relaxed);
            format!("req-{n}")
        })
}

pub fn init_tracing() -> anyhow::Result<()> {
    // 1. Resolve DEBUG_MODE and env filter
    let debug_mode = std::env::var("DEBUG_MODE")
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false);

    let mut filter = tracing_subscriber::EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info"));

    if debug_mode {
        filter = filter
            .add_directive("rust=debug".parse()?)
            .add_directive("tower_http=debug".parse()?);
    }

    // 2. Pick JSON or text formatter
    let json_logs = std::env::var("LOG_FORMAT")
        .map(|v| v.eq_ignore_ascii_case("json"))
        .unwrap_or(false);

    if json_logs {
        tracing_subscriber::fmt()
            .json()
            .with_env_filter(filter)
            .with_current_span(true)
            .with_span_list(false)
            .flatten_event(true)
            .init();
    } else {
        tracing_subscriber::fmt()
            .with_env_filter(filter)
            .with_target(true)
            .init();
    }

    // 3. Log startup metadata
    let service = std::env::var("SERVICE_NAME").unwrap_or_else(|_| "gentlix-rust".to_string());
    tracing::info!(
        service = %service,
        debug_mode,
        log_format = if json_logs { "json" } else { "text" },
        "tracing initialized"
    );

    Ok(())
}

pub fn http_trace_layer() -> TraceLayer<
    tower_http::classify::SharedClassifier<tower_http::classify::ServerErrorsAsFailures>,
    impl Fn(&Request<axum::body::Body>) -> Span + Clone + Send + 'static,
    tower_http::trace::DefaultOnRequest,
    impl Fn(&Response<axum::body::Body>, Duration, &Span) + Clone + Send + 'static,
    tower_http::trace::DefaultOnBodyChunk,
    tower_http::trace::DefaultOnEos,
    tower_http::trace::DefaultOnFailure,
> {
    TraceLayer::new_for_http()
        .make_span_with(|request: &Request<_>| {
            let request_id = request_id_from_headers(request);
            tracing::info_span!(
                "http_request",
                method = %request.method(),
                path = %request.uri().path(),
                query = %request.uri().query().unwrap_or(""),
                request_id = %request_id,
            )
        })
        .on_response(
            |response: &Response<_>, latency: Duration, _span: &Span| {
                let status = response.status().as_u16();
                let latency_ms = latency.as_millis() as u64;
                if status >= 500 {
                    tracing::error!(status, latency_ms, "request failed");
                } else if status >= 400 {
                    tracing::warn!(status, latency_ms, "request client error");
                } else {
                    tracing::info!(status, latency_ms, "request completed");
                }
            },
        )
}
