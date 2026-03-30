#ifndef C_AFFILIATE_SERVICE_H
#define C_AFFILIATE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "ids.h"
#include "affiliate.h"
#include "account.h"
#include "repo_error.h"
#include "service_error.h"
#include "affiliate_repo.h"
#include "account_repo.h"

typedef struct AffiliateRepositoryVTable {

    bool (*get)(
        void *ctx,
        UserId owner_user_id,
        AccountId recipient_sub_account_id,
        Affiliate **out,
        RepoError *err);

    bool (*list_for_owner)(
        void *ctx,
        UserId owner_user_id,
        Affiliate ***out_affiliates,
        size_t *out_count,
        RepoError *err);

    bool (*insert)(
        void *ctx,
        const Affiliate *affiliate,
        RepoError *err);

    bool (*update_nickname)(
        void *ctx,
        UserId owner_user_id,
        AccountId recipient_sub_account_id,
        const char *nickname,
        RepoError *err);

    bool (*delete_fn)(
        void *ctx,
        UserId owner_user_id,
        AccountId recipient_sub_account_id,
        RepoError *err);

    bool (*exists)(
        void *ctx,
        UserId owner_user_id,
        AccountId recipient_sub_account_id,
        bool *out_exists,
        RepoError *err);

} AffiliateRepositoryVTable;

typedef struct AffiliateRepository {
    const AffiliateRepositoryVTable *vtable;
    void *ctx;
} AffiliateRepository;

typedef struct AffSvcAccountRepositoryVTable {

    bool (*get_by_id)(
        void *ctx,
        AccountId account_id,
        Account **out,
        RepoError *err);

} AffSvcAccountRepositoryVTable;

typedef struct AffSvcAccountRepository {
    const AffSvcAccountRepositoryVTable *vtable;
    void *ctx;
} AffSvcAccountRepository;

/*
 * Concrete adapters from DB-layer repositories to these abstractions.
 *
 * These are your "ports and adapters" in C.
 */

AffiliateRepository    aff_repository_from_repo(AffiliateRepo *repo);
AffSvcAccountRepository aff_account_repository_from_repo(AccountRepo *repo);

/* ---- AffiliateService "object" --------------------------------------- */

typedef struct AffiliateService AffiliateService;

/* Constructor / Destructor */
AffiliateService *affiliate_service_new(AffiliateRepository    aff_repo,
                                        AffSvcAccountRepository account_repo);
void              affiliate_service_free(AffiliateService *svc);

/* Methods */

/*
 * create_affiliate: validate and persist a new affiliate link.
 *
 * Business rules (same as Rust):
 *   1. recipient_sub_account_id must refer to an existing account.
 *   2. The (owner, sub-account) pair must not already exist.
 *   3. nickname is validated by affiliate_new (§4.1).
 *
 * Mirrors:
 *   pub async fn create_affiliate(&self, owner_user_id, recipient_sub_account_id,
 *                                  nickname) -> ServiceResult<()>
 */
bool affiliate_service_create_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    ServiceError *err
);

/*
 * list_for_owner: return all affiliate links owned by a user.
 *
 * Caller owns the returned array; each Affiliate* must be freed with
 * affiliate_free(), then the array itself with free().
 *
 * Mirrors:
 *   pub async fn list_for_owner(&self, owner_user_id) -> ServiceResult<Vec<Affiliate>>
 */
bool affiliate_service_list_for_owner(
    AffiliateService *svc,
    UserId owner_user_id,
    Affiliate ***out_affiliates,
    size_t *out_count,
    ServiceError *err
);

/*
 * rename_affiliate: update the nickname on an existing affiliate link.
 *
 * Delegates nickname validation to the repo (which calls affiliate_new
 * internally), mirroring the Rust repo's behaviour.
 *
 * Mirrors:
 *   pub async fn rename_affiliate(&self, ..., nickname) -> ServiceResult<()>
 */
bool affiliate_service_rename_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    ServiceError *err
);

/*
 * delete_affiliate: remove an affiliate link by composite key.
 *
 * Mirrors:
 *   pub async fn delete_affiliate(&self, ...) -> ServiceResult<()>
 */
bool affiliate_service_delete_affiliate(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    ServiceError *err
);

#endif