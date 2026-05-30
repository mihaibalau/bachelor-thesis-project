#include "include/affiliate_service.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
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

/* AccountRepo adapter — get_by_id + list_for_user. */

static bool aff_acct_repo_get_by_id_adapter(
    void *ctx,
    AccountId account_id,
    Account **out,
    RepoError *err)
{
    return account_repo_get_by_id((AccountRepo *)ctx, account_id, out, err);
}

static bool aff_acct_repo_list_for_user_adapter(
    void *ctx,
    UserId user_id,
    Account ***out_accounts,
    size_t *out_count,
    RepoError *err)
{
    return account_repo_list_for_user(
        (AccountRepo *)ctx, user_id, out_accounts, out_count, err);
}

static const AffSvcAccountRepositoryVTable AFF_ACCT_REPO_VTABLE = {
    aff_acct_repo_get_by_id_adapter,
    aff_acct_repo_list_for_user_adapter
};

AffSvcAccountRepository aff_account_repository_from_repo(AccountRepo *repo) {
    AffSvcAccountRepository r;
    r.vtable = &AFF_ACCT_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

/* UserRepo adapter — get_by_id + get_by_tag. */

static bool aff_user_repo_get_by_id_adapter(
    void *ctx, UserId id, User **out, RepoError *err)
{
    return user_repo_get_by_id((UserRepo *)ctx, id, out, err);
}

static bool aff_user_repo_get_by_tag_adapter(
    void *ctx, const char *tag, User **out, RepoError *err)
{
    return user_repo_get_by_tag((UserRepo *)ctx, tag, out, err);
}

static const AffSvcUserRepositoryVTable AFF_USER_REPO_VTABLE = {
    aff_user_repo_get_by_id_adapter,
    aff_user_repo_get_by_tag_adapter
};

AffSvcUserRepository aff_user_repository_from_repo(UserRepo *repo) {
    AffSvcUserRepository r;
    r.vtable = &AFF_USER_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}

struct AffiliateService {
    AffiliateRepository     aff_repo;
    AffSvcAccountRepository account_repo;
    AffSvcUserRepository    user_repo;
};

AffiliateService *affiliate_service_new(AffiliateRepository     aff_repo,
                                        AffSvcAccountRepository account_repo,
                                        AffSvcUserRepository    user_repo)
{
    AffiliateService *svc = (AffiliateService *)malloc(sizeof *svc);
    if (!svc) return NULL;
    svc->aff_repo     = aff_repo;
    svc->account_repo = account_repo;
    svc->user_repo    = user_repo;
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

/*  View / read-model helpers + use-cases                                */

void paginated_affiliates_view_free(PaginatedAffiliatesView *view) {
    if (!view) return;
    free(view->items);
    view->items = NULL;
    view->count = 0;
}

void resolved_affiliate_target_view_free(ResolvedAffiliateTargetView *view) {
    if (!view) return;
    free(view->currencies);
    view->currencies = NULL;
    view->currency_count = 0;
}

static void str_to_lower(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (!src) { if (dst_size) dst[0] = '\0'; return; }
    for (; src[i] && i + 1 < dst_size; ++i)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static void trim_copy(char *dst, size_t dst_size, const char *src) {
    const char *s = src ? src : "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') ++s;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) --len;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, s, len);
    dst[len] = '\0';
}

/* Mirrors Rust get(): NotFound -> ServiceError::NotFound("affiliate"). */
static bool aff_get(AffiliateService *svc, UserId owner, AccountId sub,
                    Affiliate **out, ServiceError *err) {
    RepoError rerr;
    if (!svc->aff_repo.vtable->get(svc->aff_repo.ctx, owner, sub, out, &rerr)) {
        if (rerr.code == REPO_ERROR_NOT_FOUND)
            { if (err) *err = service_error_not_found("affiliate"); }
        else
            { if (err) *err = service_error_from_repo(&rerr); }
        return false;
    }
    return true;
}

/* Build an AffiliateView from an Affiliate by joining account + user. */
static bool build_affiliate_view(AffiliateService *svc, const Affiliate *aff,
                                 AffiliateView *out, ServiceError *err) {
    AccountId sub = affiliate_recipient_sub_account_id(aff);

    Account  *account = NULL;
    RepoError rerr;
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, sub, &account, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    User *user = NULL;
    if (!svc->user_repo.vtable->get_by_id(
            svc->user_repo.ctx, account_user_id(account), &user, &rerr)) {
        account_free(account);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    out->recipient_sub_account_id = sub.value;
    strncpy(out->nickname, affiliate_nickname(aff), sizeof out->nickname - 1);
    out->nickname[sizeof out->nickname - 1] = '\0';
    snprintf(out->recipient_full_name, sizeof out->recipient_full_name,
             "%s %s", user_first_name(user), user_last_name(user));
    strncpy(out->currency, currency_as_str(account_currency(account)),
            sizeof out->currency - 1);
    out->currency[sizeof out->currency - 1] = '\0';

    user_free(user);
    account_free(account);
    if (err) *err = service_error_ok();
    return true;
}

bool affiliate_service_get_affiliate_view(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    AffiliateView *out,
    ServiceError *err)
{
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "AffiliateService::get_affiliate_view: invalid arguments");
        return false;
    }

    Affiliate *aff = NULL;
    if (!aff_get(svc, owner_user_id, recipient_sub_account_id, &aff, err))
        return false;

    bool ok = build_affiliate_view(svc, aff, out, err);
    affiliate_free(aff);
    return ok;
}

static int compare_view_by_nickname(const void *a, const void *b) {
    const AffiliateView *va = (const AffiliateView *)a;
    const AffiliateView *vb = (const AffiliateView *)b;
    return strcasecmp(va->nickname, vb->nickname);
}

bool affiliate_service_list_affiliates_view(
    AffiliateService *svc,
    UserId owner_user_id,
    const ListAffiliatesParams *params,
    PaginatedAffiliatesView *out,
    ServiceError *err)
{
    if (!svc || !params || !out) {
        if (err) *err = service_error_validation(
            "AffiliateService::list_affiliates_view: invalid arguments");
        return false;
    }

    /* 1. Load the owner's affiliates. */
    Affiliate **affiliates = NULL;
    size_t      aff_count  = 0;
    RepoError   rerr;

    if (!svc->aff_repo.vtable->list_for_owner(
            svc->aff_repo.ctx, owner_user_id, &affiliates, &aff_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* 2. Enrich each into an AffiliateView. */
    AffiliateView *items = NULL;
    if (aff_count > 0) {
        items = (AffiliateView *)calloc(aff_count, sizeof(AffiliateView));
        if (!items) {
            for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
            free(affiliates);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < aff_count; ++i) {
        if (!build_affiliate_view(svc, affiliates[i], &items[i], err)) {
            for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
            free(affiliates);
            free(items);
            return false;
        }
    }
    for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
    free(affiliates);

    size_t n = aff_count;

    /* 3. Search filter (min 2 chars), case-insensitive on nickname/full name. */
    if (params->search_opt) {
        char needle[128];
        char trimmed[128];
        trim_copy(trimmed, sizeof trimmed, params->search_opt);
        str_to_lower(needle, sizeof needle, trimmed);
        if (strlen(needle) >= 2) {
            size_t w = 0;
            for (size_t i = 0; i < n; ++i) {
                char nick[128], name[160];
                str_to_lower(nick, sizeof nick, items[i].nickname);
                str_to_lower(name, sizeof name, items[i].recipient_full_name);
                if (strstr(nick, needle) || strstr(name, needle))
                    items[w++] = items[i];
            }
            n = w;
        }
    }

    /* 4. Currency filter (validate the symbol first). */
    if (params->currency_opt) {
        Currency parsed;
        DomainError derr;
        if (!currency_from_str(params->currency_opt, &parsed, &derr)) {
            free(items);
            if (err) *err = service_error_validation("invalid currency");
            return false;
        }
        const char *want = currency_as_str(parsed);
        size_t w = 0;
        for (size_t i = 0; i < n; ++i) {
            if (strcasecmp(items[i].currency, want) == 0)
                items[w++] = items[i];
        }
        n = w;
    }

    /* 5. Sort by nickname (ascending), then reverse for "desc". */
    if (n > 1) qsort(items, n, sizeof(AffiliateView), compare_view_by_nickname);
    bool desc = params->sort_opt && strcmp(params->sort_opt, "desc") == 0;
    if (desc && n > 1) {
        for (size_t i = 0, j = n - 1; i < j; ++i, --j) {
            AffiliateView tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
        }
    }

    /* 6. Pagination: page>=1, page_size in [1,100]. */
    uint32_t page = params->has_page ? params->page : 1;
    if (page < 1) page = 1;
    uint32_t page_size = params->has_page_size ? params->page_size : 20;
    if (page_size < 1)   page_size = 1;
    if (page_size > 100) page_size = 100;

    uint64_t total = (uint64_t)n;
    size_t start = (size_t)(page - 1) * (size_t)page_size;
    size_t end   = start + (size_t)page_size;
    if (end > n) end = n;

    size_t page_count = (start >= n) ? 0 : (end - start);

    AffiliateView *page_items = NULL;
    if (page_count > 0) {
        page_items = (AffiliateView *)calloc(page_count, sizeof(AffiliateView));
        if (!page_items) {
            free(items);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
        memcpy(page_items, items + start, page_count * sizeof(AffiliateView));
    }
    free(items);

    out->items     = page_items;
    out->count     = page_count;
    out->page      = page;
    out->page_size = page_size;
    out->total     = total;
    if (err) *err = service_error_ok();
    return true;
}

bool affiliate_service_resolve_target_by_tag(
    AffiliateService *svc,
    UserId owner_user_id,
    const char *tag,
    ResolvedAffiliateTargetView *out,
    ServiceError *err)
{
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "AffiliateService::resolve_target_by_tag: invalid arguments");
        return false;
    }

    char tag_trim[64];
    trim_copy(tag_trim, sizeof tag_trim, tag);
    if (tag_trim[0] == '\0') {
        if (err) *err = service_error_validation("identifier cannot be empty");
        return false;
    }

    /* Resolve the target user; NotFound -> Validation("user not found"). */
    User     *target_user = NULL;
    RepoError rerr;
    if (!svc->user_repo.vtable->get_by_tag(
            svc->user_repo.ctx, tag_trim, &target_user, &rerr)) {
        if (rerr.code == REPO_ERROR_NOT_FOUND)
            { if (err) *err = service_error_validation("user not found"); }
        else
            { if (err) *err = service_error_from_repo(&rerr); }
        return false;
    }

    UserId target_user_id = user_id(target_user);

    /* Load owner + target accounts. */
    Account **owner_accs = NULL,  **target_accs = NULL;
    size_t    owner_n    = 0,       target_n     = 0;

    if (!svc->account_repo.vtable->list_for_user(
            svc->account_repo.ctx, owner_user_id, &owner_accs, &owner_n, &rerr)) {
        user_free(target_user);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    if (!svc->account_repo.vtable->list_for_user(
            svc->account_repo.ctx, target_user_id, &target_accs, &target_n, &rerr)) {
        for (size_t i = 0; i < owner_n; ++i) account_free(owner_accs[i]);
        free(owner_accs);
        user_free(target_user);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* Owner currency set. */
    bool owner_has[CURRENCY_COUNT] = { false };
    for (size_t i = 0; i < owner_n; ++i) {
        Currency c = account_currency(owner_accs[i]);
        if ((size_t)c < CURRENCY_COUNT) owner_has[c] = true;
    }

    ResolveAffiliateCurrencyOptionView *options = NULL;
    size_t opt_count = 0;
    if (target_n > 0)
        options = (ResolveAffiliateCurrencyOptionView *)calloc(
            target_n, sizeof(ResolveAffiliateCurrencyOptionView));

    for (size_t i = 0; i < target_n; ++i) {
        Currency c = account_currency(target_accs[i]);
        if ((size_t)c < CURRENCY_COUNT && owner_has[c]) {
            strncpy(options[opt_count].currency, currency_as_str(c),
                    sizeof options[opt_count].currency - 1);
            options[opt_count].currency[sizeof options[opt_count].currency - 1] = '\0';
            options[opt_count].recipient_sub_account_id = account_id(target_accs[i]).value;
            opt_count++;
        }
    }

    for (size_t i = 0; i < owner_n; ++i)  account_free(owner_accs[i]);
    for (size_t i = 0; i < target_n; ++i) account_free(target_accs[i]);
    free(owner_accs);
    free(target_accs);

    if (opt_count == 0) {
        free(options);
        user_free(target_user);
        if (err) *err = service_error_validation(
            "no compatible currencies between owner and target user");
        return false;
    }

    out->recipient_user_id = target_user_id.value;
    snprintf(out->recipient_full_name, sizeof out->recipient_full_name,
             "%s %s", user_first_name(target_user), user_last_name(target_user));
    out->currencies     = options;
    out->currency_count = opt_count;

    user_free(target_user);
    if (err) *err = service_error_ok();
    return true;
}