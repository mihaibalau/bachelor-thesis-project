use std::{net::SocketAddr, sync::Arc};

use axum::Router;
mod db;
mod domain;
mod service;
mod server;

use crate::{
    db::{
        user_repo::UserRepo,
        account_repo::AccountRepo,
        transaction_repo::TransactionRepo,
        affiliate_repo::AffiliateRepo,
        Db,
    },
    server::{
        router::create_router,
        state::{AppState, UserSvc, AccountSvc, TxSvc, AffiliateSvc, DashboardSvc},
    },
    service::{
        user::UserService,
        account::AccountService,
        transaction::TransactionService,
        affiliate::AffiliateService,
        dashboard::DashboardService,
    },
};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    dotenvy::dotenv().ok();

    // 1. Tracing (RUST_LOG, DEBUG_MODE, LOG_FORMAT=json for AWS CloudWatch)
    server::logging::init_tracing()?;

    // 2. DB pool
    let database_url = std::env::var("DATABASE_URL")
        .expect("DATABASE_URL must be set — copy .env.example to .env");

    let db = Db::new(&database_url).await?;

    // 3. Repositories
    let user_repo      = Arc::new(UserRepo::new(db.clone()));
    let account_repo   = Arc::new(AccountRepo::new(db.clone()));
    let tx_repo        = Arc::new(TransactionRepo::new(db.clone()));
    let affiliate_repo = Arc::new(AffiliateRepo::new(db.clone()));

    // 4. Services
    let user_svc: Arc<UserSvc> = Arc::new(UserService::new(
        user_repo.clone(),
        account_repo.clone(),
    ));
    let account_svc: Arc<AccountSvc> = Arc::new(AccountService::new(
        account_repo.clone(),
    ));
    let tx_svc: Arc<TxSvc> = Arc::new(TransactionService::new(
        tx_repo.clone(),
        account_repo.clone(),
    ));
    let affiliate_svc: Arc<AffiliateSvc> = Arc::new(AffiliateService::new(
        affiliate_repo.clone(),
        account_repo.clone(),
        user_repo.clone(),
    ));
    let dashboard_svc: Arc<DashboardSvc> = Arc::new(DashboardService::new(
        user_svc.clone(),
        tx_svc.clone(),
        affiliate_svc.clone(),
    ));

    // 5. AppState + router
    let jwt_secret = std::env::var("JWT_SECRET").expect("JWT_SECRET must be set");
    if jwt_secret.len() < 32 {
        anyhow::bail!("JWT_SECRET must be at least 32 bytes long");
    }

    let state = Arc::new(AppState::new(
        user_svc,
        account_svc,
        tx_svc,
        affiliate_svc,
        dashboard_svc,
        jwt_secret,
    ));
    let app: Router = create_router(state);

    // 6. Serve
    let port: u16 = std::env::var("PORT")
        .ok()
        .and_then(|p| p.parse().ok())
        .unwrap_or(6767);
    let addr = SocketAddr::from(([0, 0, 0, 0], port));
    let service = std::env::var("SERVICE_NAME").unwrap_or_else(|_| "gentlix-rust".to_string());
    tracing::info!(service = %service, %addr, "listening");

    axum::serve(
        tokio::net::TcpListener::bind(addr).await?,
        app,
    )
        .await?;

    Ok(())
}