use std::sync::Arc;

use crate::{
    db::{
        user_repo::UserRepo,
        account_repo::AccountRepo,
        transaction_repo::TransactionRepo,
        affiliate_repo::AffiliateRepo,
    },
    service::{
        user::UserService,
        account::AccountService,
        transaction::TransactionService,
        affiliate::AffiliateService,
    },
};

// Service type aliases wired into AppState
pub type UserSvc = UserService<UserRepo, AccountRepo>;
pub type AccountSvc = AccountService<AccountRepo>;
pub type TxSvc = TransactionService<TransactionRepo, AccountRepo>;
pub type AffiliateSvc = AffiliateService<AffiliateRepo, AccountRepo, UserRepo>;

pub struct AppState {
    pub user_svc: Arc<UserSvc>,
    pub account_svc: Arc<AccountSvc>,
    pub tx_svc: Arc<TxSvc>,
    pub affiliate_svc: Arc<AffiliateSvc>,
    pub jwt_secret: String,
}

impl AppState {
    pub fn new(
        user_svc: Arc<UserSvc>,
        account_svc: Arc<AccountSvc>,
        tx_svc: Arc<TxSvc>,
        affiliate_svc: Arc<AffiliateSvc>,
        jwt_secret: String
    ) -> Self {
        Self {
            user_svc,
            account_svc,
            tx_svc,
            affiliate_svc,
            jwt_secret,
        }
    }
}