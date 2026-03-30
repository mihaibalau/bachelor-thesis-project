#include "include/affiliate_service.h"

#include <stdlib.h>
#include <string.h>

static bool aff_repo_get_adapter(
    void *ctx,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    Affiliate **out,
    RepoError *err)
{
    return affiliate_repo_get(
        (AffiliateRepo *)ctx,
        owner_user_id,
        recipient_sub_account_id,
        out,
        err);
}

static bool aff_repo_list_for_owner_adapter(
    void *ctx,
    UserId owner_user_id,
    Affiliate ***out_affiliates,
    size_t *out_count,
    RepoError *err)
{
    return affiliate_repo_list_for_owner(
        (AffiliateRepo *)ctx,
        owner_user_id,
        out_affiliates,
        out_count,
        err);
}

static bool aff_repo_insert_adapter(
    void *ctx,
    const Affiliate *affiliate,
    RepoError *err)
{
    return affiliate_repo_insert((AffiliateRepo *)ctx, affiliate, err);
}

static bool aff_repo_update_nickname_adapter(
    void *ctx,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    RepoError *err)
{
    return affiliate_repo_update_nickname(
        (AffiliateRepo *)ctx,
        owner_user_id,
        recipient_sub_account_id,
        nickname,
        err);
}

static bool aff_repo_delete_adapter(
    void *ctx,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    RepoError *err)
{
    return affiliate_repo_delete(
        (AffiliateRepo *)ctx,
        owner_user_id,
        recipient_sub_account_id,
        err);
}

static bool aff_repo_exists_adapter(
    void *ctx,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    bool *out_exists,
    RepoError *err)
{
    return affiliate_repo_exists(
        (AffiliateRepo *)ctx,
        owner_user_id,
        recipient_sub_account_id,
        out_exists,
        err);
}

/*
 * Static, read-only vtable — one instance shared by all repos.
 * §3.3: the vtable IS the trait object in C.
 */
static const AffiliateRepositoryVTable AFF_REPO_VTABLE = {
    aff_repo_get_adapter,
    aff_repo_list_for_owner_adapter,
    aff_repo_insert_adapter,
    aff_repo_update_nickname_adapter,
    aff_repo_delete_adapter,
    aff_repo_exists_adapter
};

AffiliateRepository aff_repository_from_repo(AffiliateRepo *repo) {
    AffiliateRepository r;
    r.vtable = &AFF_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

/* AccountRepo adapter — only get_by_id is needed here. */

static bool aff_acct_repo_get_by_id_adapter(
    void *ctx,
    AccountId account_id,
    Account **out,
    RepoError *err)
{
    return account_repo_get_by_id((AccountRepo *)ctx, account_id, out, err);
}

static const AffSvcAccountRepositoryVTable AFF_ACCT_REPO_VTABLE = {
    aff_acct_repo_get_by_id_adapter
};

AffSvcAccountRepository aff_account_repository_from_repo(AccountRepo *repo) {
    AffSvcAccountRepository r;
    r.vtable = &AFF_ACCT_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

struct AffiliateService {
    AffiliateRepository    aff_repo;
    AffSvcAccountRepository account_repo;
};

AffiliateService *affiliate_service_new(AffiliateRepository    aff_repo,
                                        AffSvcAccountRepository account_repo)
{
    AffiliateService *svc = (AffiliateService *)malloc(sizeof *svc);
    if (!svc) return NULL;
    svc->aff_repo     = aff_repo;
    svc->account_repo = account_repo;
    return svc;
}

void affiliate_service_free(AffiliateService *svc) {
    free(svc);
}

/*  Public methods                                                       */

bool affiliate_service_create_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    ServiceError *err)
{
    if (!svc || !nickname) {
        if (err) *err = service_error_validation(
            "AffiliateService::create_affiliate: invalid arguments");
        return false;
    }

    /*
     * Step 1: verify the recipient account actually exists.
     *
     * §4.2 Error Propagation: RepoError::NotFound is mapped to a
     * ServiceError::Validation (not NotFound) because from the caller's
     * perspective "you tried to create an affiliate for a non-existing
     * account" is a validation mistake, not a missing-affiliate error.
     *
     * Mirrors Rust:
     *   self.account_repo.get_by_id(recipient_sub_account_id)
     *       .await
     *       .map_err(|e| match e {
     *           RepoError::NotFound(_) =>
     *               ServiceError::Validation("cannot create affiliate ..."),
     *           other => ServiceError::from(other),
     *       })?;
     */
    Account  *tmp_account = NULL;
    RepoError rerr;

    bool account_ok = svc->account_repo.vtable->get_by_id(
        svc->account_repo.ctx,
        recipient_sub_account_id,
        &tmp_account,
        &rerr);

    if (!account_ok) {
        if (rerr.code == REPO_ERROR_NOT_FOUND) {
            if (err) *err = service_error_validation(
                "cannot create affiliate for non-existing account");
        } else {
            if (err) *err = service_error_from_repo(&rerr);
        }
        return false;
    }

    /* We only needed existence — free the fetched account immediately. */
    account_free(tmp_account);

    /*
     * Step 2: prevent duplicate links.
     *
     * Mirrors Rust:
     *   if self.affiliate_repo.exists(owner_user_id, recipient_sub_account_id).await? {
     *       return Err(ServiceError::Conflict { entity: "affiliate", ... });
     *   }
     */
    bool already_exists = false;
    if (!svc->aff_repo.vtable->exists(
            svc->aff_repo.ctx,
            owner_user_id,
            recipient_sub_account_id,
            &already_exists,
            &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (already_exists) {
        if (err) *err = service_error_conflict(
            "affiliate",
            "affiliate already exists for this account");
        return false;
    }

    /*
     * Step 3: build the domain object.
     *
     * §4.1 Type-Driven Invariants: affiliate_new trims and validates the
     * nickname — empty or whitespace-only values are rejected here, before
     * any SQL is executed.
     *
     * Mirrors Rust:
     *   let affiliate = Affiliate::new(owner_user_id,
     *                                   recipient_sub_account_id, nickname)?;
     */
    DomainError derr;
    Affiliate *affiliate = affiliate_new(
        owner_user_id,
        recipient_sub_account_id,
        nickname,
        &derr);

    if (!affiliate) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    /*
     * Step 4: persist through the repository interface.
     *
     * Mirrors Rust:
     *   self.affiliate_repo.insert(&affiliate).await?;
     */
    bool ok = svc->aff_repo.vtable->insert(
        svc->aff_repo.ctx, affiliate, &rerr);

    affiliate_free(affiliate);

    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}

bool affiliate_service_list_for_owner(
    AffiliateService *svc,
    UserId owner_user_id,
    Affiliate ***out_affiliates,
    size_t *out_count,
    ServiceError *err)
{
    if (!svc || !out_affiliates || !out_count) {
        if (err) *err = service_error_validation(
            "AffiliateService::list_for_owner: invalid arguments");
        return false;
    }

    /*
     * Pure delegation to the repository.
     *
     * Mirrors Rust:
     *   pub async fn list_for_owner(&self, owner_user_id) -> ServiceResult<Vec<Affiliate>> {
     *       let list = self.affiliate_repo.list_for_owner(owner_user_id).await?;
     *       Ok(list)
     *   }
     */
    RepoError rerr;
    bool ok = svc->aff_repo.vtable->list_for_owner(
        svc->aff_repo.ctx,
        owner_user_id,
        out_affiliates,
        out_count,
        &rerr);

    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}

bool affiliate_service_rename_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    ServiceError *err)
{
    if (!svc || !nickname) {
        if (err) *err = service_error_validation(
            "AffiliateService::rename_affiliate: invalid arguments");
        return false;
    }

    /*
     * The repo's update_nickname already calls affiliate_new internally
     * to validate the nickname before running the SQL UPDATE.
     * Domain validation is therefore enforced at the repo boundary —
     * the service does not need to repeat it.
     *
     * §4.2: RepoError::NotFound is converted to ServiceError::NotFound
     * so the caller can distinguish "link doesn't exist" from infrastructure
     * failures without seeing RepoError directly.
     *
     * Mirrors Rust:
     *   pub async fn rename_affiliate(&self, ..., nickname) -> ServiceResult<()> {
     *       self.affiliate_repo
     *           .update_nickname(owner_user_id, recipient_sub_account_id, &nickname)
     *           .await?;
     *       Ok(())
     *   }
     */
    RepoError rerr;
    bool ok = svc->aff_repo.vtable->update_nickname(
        svc->aff_repo.ctx,
        owner_user_id,
        recipient_sub_account_id,
        nickname,
        &rerr);

    if (!ok) {
        if (rerr.code == REPO_ERROR_NOT_FOUND) {
            if (err) *err = service_error_not_found("affiliate");
        } else {
            if (err) *err = service_error_from_repo(&rerr);
        }
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}

bool affiliate_service_delete_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    ServiceError *err)
{
    if (!svc) {
        if (err) *err = service_error_validation(
            "AffiliateService::delete_affiliate: invalid arguments");
        return false;
    }

    /*
     * §4.2: translate RepoError::NotFound -> ServiceError::NotFound.
     * All other repo errors bubble up as ServiceError::Repo.
     *
     * Mirrors Rust:
     *   pub async fn delete_affiliate(&self, ...) -> ServiceResult<()> {
     *       self.affiliate_repo.delete(owner_user_id, recipient_sub_account_id).await?;
     *       Ok(())
     *   }
     */
    RepoError rerr;
    bool ok = svc->aff_repo.vtable->delete_fn(
        svc->aff_repo.ctx,
        owner_user_id,
        recipient_sub_account_id,
        &rerr);

    if (!ok) {
        if (rerr.code == REPO_ERROR_NOT_FOUND) {
            if (err) *err = service_error_not_found("affiliate");
        } else {
            if (err) *err = service_error_from_repo(&rerr);
        }
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}