use std::sync::Arc;
use async_trait::async_trait;
use crate::db::affiliate_repo::AffiliateRepo;
use crate::db::errors::RepoError;
use crate::domain::affiliate::Affiliate;
use crate::domain::ids::{AccountId, UserId};
use crate::service::account::AccountRepository;
use crate::service::errors::{ServiceError, ServiceResult};

/// Repository abstraction for affiliates
///
/// Defining this as a trait lets you unit-test `AccountService` with
/// in-memory fakes instead of hitting Postgres
#[async_trait]
pub trait AffiliateRepository: Send + Sync {
    async fn get(&self, owner_used_id: UserId, recipient_sub_account_id: AccountId) -> Result<Affiliate, RepoError>;
    async fn list_for_owner(&self, owner_user_id: UserId) -> Result<Vec<Affiliate>, RepoError>;
    async fn insert(&self, affiliate: &Affiliate) -> Result<(), RepoError>;
    async fn update_nickname(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: &str) -> Result<(), RepoError>;
    async fn delete(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<(), RepoError>;
    async fn exists(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<bool, RepoError>;
}

/// Thin adapter: your concrete SQLx repo implements the trait
///
/// This mirrors the "port/adapter" pattern and keeps the service layer
/// decoupled from SQLx's exact API
#[async_trait]
impl AffiliateRepository for AffiliateRepo {
    async fn get(&self, owner_used_id: UserId, recipient_sub_account_id: AccountId) -> Result<Affiliate, RepoError> {
        self.get(owner_used_id, recipient_sub_account_id).await
    }

    async fn list_for_owner(&self, owner_user_id: UserId) -> Result<Vec<Affiliate>, RepoError> {
        self.list_for_owner(owner_user_id).await
    }

    async fn insert(&self, affiliate: &Affiliate) -> Result<(), RepoError> {
        self.insert(affiliate).await
    }

    async fn update_nickname(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: &str) -> Result<(), RepoError> {
        self.update_nickname(owner_user_id, recipient_sub_account_id, nickname).await
    }

    async fn delete(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<(), RepoError> {
        self.delete(owner_user_id, recipient_sub_account_id).await
    }

    async fn exists(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<bool, RepoError> {
        self.exists(owner_user_id, recipient_sub_account_id).await
    }
}

#[derive(Clone)]
pub struct AffiliateService<R, A>
where
    R: AffiliateRepository,
    A: AccountRepository,
{
    affiliate_repo: Arc<R>,
    account_repo: Arc<A>,
}

impl<R, A> AffiliateService<R, A>
where
    R: AffiliateRepository,
    A: AccountRepository,
{
    pub fn new(affiliate_repo: Arc<R>, account_repo: Arc<A>) -> Self {
        Self { affiliate_repo, account_repo }
    }

    /// Create a new affiliate link.
    ///
    /// Business rules:
    /// - referenced sub-account must exist,
    /// - prevent duplicates using `exists`
    pub async fn create_affiliate(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: String, )
        -> ServiceResult<()> {
        // Ensure the recipient account exists
        let _ = self
            .account_repo
            .get_by_id(recipient_sub_account_id)
            .await
            .map_err(|e| match e {
                RepoError::NotFound(_) => ServiceError::Validation(
                    "cannot create affiliate for non-existing account".into(),
                ),
                other => ServiceError::from(other),
            })?;

        // Check if the affiliate already exists
        if self
            .affiliate_repo
            .exists(owner_user_id, recipient_sub_account_id)
            .await?
        {
            return Err(ServiceError::Conflict {
                entity: "affiliate",
                message: "affiliate already exists for this account".into(),
            });
        }

        // Create and persist the affiliate
        let affiliate = Affiliate::new(owner_user_id, recipient_sub_account_id, nickname)?;
        self.affiliate_repo.insert(&affiliate).await?;
        Ok(())
    }


    pub async fn list_for_owner(&self, owner_user_id: UserId) -> ServiceResult<Vec<Affiliate>> {
        let list = self.affiliate_repo.list_for_owner(owner_user_id).await?;
        Ok(list)
    }

    pub async fn rename_affiliate(
        &self,
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
        nickname: String,
    ) -> ServiceResult<()> {
        self.affiliate_repo
            .update_nickname(owner_user_id, recipient_sub_account_id, &nickname)
            .await?;
        Ok(())
    }

    pub async fn delete_affiliate(
        &self,
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
    ) -> ServiceResult<()> {
        self.affiliate_repo
            .delete(owner_user_id, recipient_sub_account_id)
            .await?;
        Ok(())
    }

    pub async fn get(
        &self,
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
    ) -> ServiceResult<Affiliate> {
        match self
            .affiliate_repo
            .get(owner_user_id, recipient_sub_account_id)
            .await
        {
            Ok(a) => Ok(a),
            Err(RepoError::NotFound(_)) => Err(ServiceError::not_found("affiliate")),
            Err(e) => Err(ServiceError::from(e)),
        }
    }
}