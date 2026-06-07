use std::sync::Arc;
use async_trait::async_trait;
use crate::db::affiliate_repo::AffiliateRepo;
use crate::db::errors::RepoError;
use crate::domain::affiliate::Affiliate;
use crate::domain::ids::{AccountId, UserId};
use crate::service::account::AccountRepository;
use crate::service::errors::{ServiceError, ServiceResult};
use crate::service::user::UserRepository;
use crate::domain::value::currency::Currency;
use std::collections::HashSet;
use serde::Serialize;

// Service-level DTOs for affiliates (consumed by routes)
#[derive(Debug, Clone, Serialize)]
pub struct AffiliateView {
    pub recipient_sub_account_id: i64,
    pub nickname: String,
    pub recipient_full_name: String,
    pub currency: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct PaginatedAffiliatesView {
    pub items: Vec<AffiliateView>,
    pub page: u32,
    pub page_size: u32,
    pub total: u64,
}

#[derive(Debug, Clone, Serialize)]
pub struct ResolveAffiliateCurrencyOptionView {
    pub currency: String,
    pub recipient_sub_account_id: i64,
}

#[derive(Debug, Clone, Serialize)]
pub struct ResolvedAffiliateTargetView {
    pub recipient_user_id: i64,
    pub recipient_full_name: String,
    pub currencies: Vec<ResolveAffiliateCurrencyOptionView>,
}

#[derive(Debug, Clone)]
pub struct ListAffiliatesParams {
    pub page: Option<u32>,
    pub page_size: Option<u32>,
    pub search: Option<String>,
    pub currency: Option<String>,
    pub for_send_currency: Option<String>,
    pub sort: Option<String>, // "asc" | "desc"
}

// Repository port for affiliates
#[async_trait]
pub trait AffiliateRepository: Send + Sync {
    async fn get(&self, owner_used_id: UserId, recipient_sub_account_id: AccountId) -> Result<Affiliate, RepoError>;
    async fn list_for_owner(&self, owner_user_id: UserId) -> Result<Vec<Affiliate>, RepoError>;
    async fn insert(&self, affiliate: &Affiliate) -> Result<(), RepoError>;
    async fn update_nickname(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: &str) -> Result<(), RepoError>;
    async fn delete(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<(), RepoError>;
    async fn exists(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> Result<bool, RepoError>;
}

// SQLx adapter implementing AffiliateRepository
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
pub struct AffiliateService<R, A, U>
where
    R: AffiliateRepository,
    A: AccountRepository,
    U: UserRepository,
{
    affiliate_repo: Arc<R>,
    account_repo: Arc<A>,
    user_repo: Arc<U>,
}

impl<R, A, U> AffiliateService<R, A, U>
where
    R: AffiliateRepository,
    A: AccountRepository,
    U: UserRepository,
{
    pub fn new(affiliate_repo: Arc<R>, account_repo: Arc<A>, user_repo: Arc<U>) -> Self {
        Self { affiliate_repo, account_repo, user_repo }
    }

    pub async fn create_affiliate(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId, nickname: String, )
        -> ServiceResult<()> {
        // 1. Ensure recipient account exists
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

        // 2. Reject duplicate affiliate
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

        // 3. Create and persist affiliate
        let affiliate = Affiliate::new(owner_user_id, recipient_sub_account_id, nickname)?;
        self.affiliate_repo.insert(&affiliate).await?;
        Ok(())
    }


    pub async fn list_for_owner(&self, owner_user_id: UserId) -> ServiceResult<Vec<Affiliate>> {
        let list = self.affiliate_repo.list_for_owner(owner_user_id).await?;
        Ok(list)
    }

    pub async fn validate_send_target(
        &self,
        owner_user_id: UserId,
        from_account_id: AccountId,
        recipient_account_id: AccountId,
    ) -> ServiceResult<()> {
        // 1. Load sender and recipient accounts
        let from_acc = self.account_repo.get_by_id(from_account_id).await?;
        let to_acc = self.account_repo.get_by_id(recipient_account_id).await?;

        // 2. Enforce Regular account + matching currency
        if to_acc.account_type() != crate::domain::value::account_type::AccountType::Regular {
            return Err(ServiceError::Validation(
                "recipient must be a Regular account".into(),
            ));
        }

        if from_acc.currency() != to_acc.currency() {
            return Err(ServiceError::Validation(format!(
                "This affiliate does not have a Regular {} account.",
                from_acc.currency().as_str()
            )));
        }

        // 3. Confirm recipient user is a saved affiliate
        let affiliates = self.affiliate_repo.list_for_owner(owner_user_id).await?;
        let recipient_user_id = to_acc.user_id();

        for a in affiliates {
            let saved = self.account_repo.get_by_id(a.recipient_sub_account_id()).await?;
            if saved.user_id() == recipient_user_id {
                return Ok(());
            }
        }

        Err(ServiceError::Validation(
            "recipient is not one of your affiliates".into(),
        ))
    }

    pub async fn rename_affiliate(
        &self,
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
        nickname: String,
    ) -> ServiceResult<()> {
        self.affiliate_repo
            .update_nickname(owner_user_id, recipient_sub_account_id, &nickname)
            .await
            .map_err(|e| match e {
                RepoError::NotFound(entity) => ServiceError::not_found(entity),
                e => ServiceError::Repo(e),
            })?;
        Ok(())
    }

    pub async fn delete_affiliate(
        &self,
        owner_user_id: UserId,
        recipient_sub_account_id: AccountId,
    ) -> ServiceResult<()> {
        self.affiliate_repo
            .delete(owner_user_id, recipient_sub_account_id)
            .await
            .map_err(|e| match e {
                RepoError::NotFound(entity) => ServiceError::not_found(entity),
                e => ServiceError::Repo(e),
            })?;
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

    #[tracing::instrument(skip(self), fields(owner_user_id = %owner_user_id.0, page, page_size))]
    pub async fn list_affiliates_view(
        &self,
        owner_user_id: UserId,
        params: ListAffiliatesParams,
    ) -> ServiceResult<PaginatedAffiliatesView> {
        // 1. Load affiliates and build view rows
        let affiliates = self.affiliate_repo.list_for_owner(owner_user_id).await?;

        let mut items = Vec::with_capacity(affiliates.len());
        let mut seen_nicknames: HashSet<String> = HashSet::new();

        for a in affiliates {
            let saved_account = self.account_repo.get_by_id(a.recipient_sub_account_id()).await?;
            let user = self.user_repo.get_by_id(saved_account.user_id()).await?;
            let full_name = format!("{} {}", user.first_name(), user.last_name());

            let (recipient_sub_account_id, currency) = if let Some(curr_str) = params.for_send_currency.as_ref() {
                use core::str::FromStr;
                let up = curr_str.to_uppercase();
                let _curr = Currency::from_str(&up)
                    .map_err(|_| ServiceError::Validation("invalid currency".to_string()))?;

                let nick_key = a.nickname().to_lowercase();
                if seen_nicknames.contains(&nick_key) {
                    continue;
                }

                let target_accounts = self.account_repo.list_for_user(saved_account.user_id()).await?;
                let target = target_accounts.into_iter().find(|acc| {
                    acc.account_type() == crate::domain::value::account_type::AccountType::Regular
                        && acc.currency().as_str().eq_ignore_ascii_case(&up)
                });

                let Some(target) = target else { continue };
                seen_nicknames.insert(nick_key);
                (target.id().unwrap().0, up)
            } else {
                (
                    a.recipient_sub_account_id().0,
                    saved_account.currency().as_str().to_string(),
                )
            };

            items.push(AffiliateView {
                recipient_sub_account_id,
                nickname: a.nickname().to_string(),
                recipient_full_name: full_name,
                currency,
            });
        }

        // 2. Apply search filter (min 2 chars)
        let mut filtered = items;
        if let Some(s) = params.search.as_ref() {
            let s_trim = s.trim().to_lowercase();
            if s_trim.len() >= 2 {
                filtered = filtered
                    .into_iter()
                    .filter(|item| {
                        let nick = item.nickname.to_lowercase();
                        let name = item.recipient_full_name.to_lowercase();
                        nick.contains(&s_trim) || name.contains(&s_trim)
                    })
                    .collect();
            }
        }

        // 3. Filter by currency (skipped when for_send_currency resolved targets)
        if params.for_send_currency.is_none() {
        if let Some(curr_str) = params.currency.as_ref() {
            use core::str::FromStr;
            let _curr = Currency::from_str(curr_str)
                .map_err(|_| ServiceError::Validation("invalid currency".to_string()))?;
            let up = curr_str.to_uppercase();
            filtered = filtered
                .into_iter()
                .filter(|item| item.currency.eq_ignore_ascii_case(&up))
                .collect();
        }
        }

        // 4. Sort and paginate
        let sort_dir = params.sort.as_deref().unwrap_or("asc");
        filtered.sort_by(|a, b| {
            let ord = a.nickname.to_lowercase().cmp(&b.nickname.to_lowercase());
            if sort_dir == "desc" { ord.reverse() } else { ord }
        });

        let page = params.page.unwrap_or(1).max(1);
        let page_size = params.page_size.unwrap_or(20).clamp(1, 100);
        let total = filtered.len() as u64;
        let start = ((page - 1) * page_size) as usize;
        let page_items = if start >= filtered.len() {
            Vec::new()
        } else {
            filtered.into_iter().skip(start).take(page_size as usize).collect()
        };

        Ok(PaginatedAffiliatesView { items: page_items, page, page_size, total })
    }

    #[tracing::instrument(skip(self), fields(owner_user_id = %owner_user_id.0, sub_account_id = %recipient_sub_account_id.0))]
    pub async fn get_affiliate_view(&self, owner_user_id: UserId, recipient_sub_account_id: AccountId) -> ServiceResult<AffiliateView> {
        let affiliate = self.get(owner_user_id, recipient_sub_account_id).await?;
        let account = self.account_repo.get_by_id(affiliate.recipient_sub_account_id()).await?;
        let user = self.user_repo.get_by_id(account.user_id()).await?;
        let full_name = format!("{} {}", user.first_name(), user.last_name());
        Ok(AffiliateView {
            recipient_sub_account_id: affiliate.recipient_sub_account_id().0,
            nickname: affiliate.nickname().to_string(),
            recipient_full_name: full_name,
            currency: account.currency().as_str().to_string(),
        })
    }

    #[tracing::instrument(skip(self), fields(owner_user_id = %owner_user_id.0, tag = %tag))]
    pub async fn resolve_target_by_tag(&self, owner_user_id: UserId, tag: String) -> ServiceResult<ResolvedAffiliateTargetView> {
        // 1. Resolve user by tag
        let tag = tag.trim().to_string();
        if tag.is_empty() {
            return Err(ServiceError::Validation("identifier cannot be empty".into()));
        }
        let target_user = self.user_repo.get_by_tag(&tag).await.map_err(|e| match e {
            RepoError::NotFound(_) => ServiceError::Validation(format!(
                "No Gentlix user was found with tag \"{tag}\". Check the spelling and try again."
            )),
            other => ServiceError::from(other),
        })?;
        let target_user_id = target_user.id().ok_or_else(|| ServiceError::Validation("target user has no id in memory".to_string()))?;

        // 2. Match Regular accounts in currencies the owner can send
        let owner_accounts = self.account_repo.list_for_user(owner_user_id).await?;
        let target_accounts = self.account_repo.list_for_user(target_user_id).await?;

        let owner_currencies: HashSet<_> = owner_accounts.iter().map(|a| a.currency()).collect();

        let mut options = Vec::new();
        for acc in target_accounts {
            if acc.account_type() != crate::domain::value::account_type::AccountType::Regular {
                continue;
            }
            let curr = acc.currency();
            if owner_currencies.contains(&curr) {
                options.push(ResolveAffiliateCurrencyOptionView {
                    currency: curr.as_str().to_string(),
                    recipient_sub_account_id: acc.id().unwrap().0,
                });
            }
        }
        if options.is_empty() {
            return Err(ServiceError::Validation(
                "This user has no Regular account in a currency you can send to. Transfers can only be made to Regular accounts.".into(),
            ));
        }
        let full_name = format!("{} {}", target_user.first_name(), target_user.last_name());
        Ok(ResolvedAffiliateTargetView { recipient_user_id: target_user_id.0, recipient_full_name: full_name, currencies: options })
    }
}