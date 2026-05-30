#ifndef C_AFFILIATE_SERVICE_H
#define C_AFFILIATE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ids.h"
#include "affiliate.h"
#include "account.h"
#include "user.h"
#include "repo_error.h"
#include "service_error.h"
#include "affiliate_repo.h"
#include "account_repo.h"
#include "user_repo.h"

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

    /* Needed by resolve_target_by_tag to compare owner/target currencies. */
    bool (*list_for_user)(
        void *ctx,
        UserId user_id,
        Account ***out_accounts,
        size_t *out_count,
        RepoError *err);

} AffSvcAccountRepositoryVTable;

typedef struct AffSvcAccountRepository {
    const AffSvcAccountRepositoryVTable *vtable;
    void *ctx;
} AffSvcAccountRepository;

/*
 * User repository port — the affiliate views need the recipient's name
 * (get_by_id) and tag-based resolution (get_by_tag), mirroring Rust's
 * AffiliateService<R, A, U> which composes a UserRepository.
 */
typedef struct AffSvcUserRepositoryVTable {

    bool (*get_by_id)(
        void *ctx,
        UserId id,
        User **out,
        RepoError *err);

    bool (*get_by_tag)(
        void *ctx,
        const char *tag,
        User **out,
        RepoError *err);

} AffSvcUserRepositoryVTable;

typedef struct AffSvcUserRepository {
    const AffSvcUserRepositoryVTable *vtable;
    void *ctx;
} AffSvcUserRepository;

/*
 * Concrete adapters from DB-layer repositories to these abstractions.
 *
 * These are your "ports and adapters" in C.
 */

AffiliateRepository    aff_repository_from_repo(AffiliateRepo *repo);
AffSvcAccountRepository aff_account_repository_from_repo(AccountRepo *repo);
AffSvcUserRepository    aff_user_repository_from_repo(UserRepo *repo);

/* ---- Service-level read models (DTOs consumed by the HTTP layer) ----- */

typedef struct AffiliateView {
    int64_t recipient_sub_account_id;
    char    nickname[64];
    char    recipient_full_name[128];
    char    currency[8];
} AffiliateView;

typedef struct PaginatedAffiliatesView {
    AffiliateView *items;       /* caller frees with paginated_affiliates_view_free */
    size_t         count;
    uint32_t       page;
    uint32_t       page_size;
    uint64_t       total;
} PaginatedAffiliatesView;

typedef struct ResolveAffiliateCurrencyOptionView {
    char    currency[8];
    int64_t recipient_sub_account_id;
} ResolveAffiliateCurrencyOptionView;

typedef struct ResolvedAffiliateTargetView {
    int64_t recipient_user_id;
    char    recipient_full_name[128];
    ResolveAffiliateCurrencyOptionView *currencies; /* caller frees with resolved_affiliate_target_view_free */
    size_t  currency_count;
} ResolvedAffiliateTargetView;

/*
 * Optional query parameters for list_affiliates_view. Use the has_* flags to
 * express Rust's Option<u32>; string pointers are NULL when absent.
 */
typedef struct ListAffiliatesParams {
    bool        has_page;
    uint32_t    page;
    bool        has_page_size;
    uint32_t    page_size;
    const char *search_opt;     /* NULL or borrowed string */
    const char *currency_opt;   /* NULL or borrowed string */
    const char *sort_opt;       /* NULL, "asc" or "desc"   */
} ListAffiliatesParams;

void paginated_affiliates_view_free(PaginatedAffiliatesView *view);
void resolved_affiliate_target_view_free(ResolvedAffiliateTargetView *view);

/* ---- AffiliateService "object" --------------------------------------- */

typedef struct AffiliateService AffiliateService;

/* Constructor / Destructor */
AffiliateService *affiliate_service_new(AffiliateRepository     aff_repo,
                                        AffSvcAccountRepository account_repo,
                                        AffSvcUserRepository    user_repo);
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

/*
 * get_affiliate_view: load one affiliate plus the recipient's full name and
 * the sub-account's currency.
 *
 * Mirrors Rust AffiliateService::get_affiliate_view.
 */
bool affiliate_service_get_affiliate_view(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    AffiliateView *out,
    ServiceError *err
);

/*
 * list_affiliates_view: list the owner's affiliates as enriched views, then
 * apply search (min 2 chars), currency filter, nickname sort and pagination —
 * all exactly as Rust's list_affiliates_view does.
 *
 * On success the caller owns *out and must release it with
 * paginated_affiliates_view_free.
 */
bool affiliate_service_list_affiliates_view(
    AffiliateService *svc,
    UserId owner_user_id,
    const ListAffiliatesParams *params,
    PaginatedAffiliatesView *out,
    ServiceError *err
);

/*
 * resolve_target_by_tag: resolve a recipient user by tag and return the
 * currencies they share with the owner (so the UI can pick a sub-account).
 *
 * Mirrors Rust AffiliateService::resolve_target_by_tag.
 * On success the caller owns *out and must release it with
 * resolved_affiliate_target_view_free.
 */
bool affiliate_service_resolve_target_by_tag(
    AffiliateService *svc,
    UserId owner_user_id,
    const char *tag,
    ResolvedAffiliateTargetView *out,
    ServiceError *err
);

#endif