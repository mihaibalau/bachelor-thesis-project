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

// AccountRepo adapter.

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

// UserRepo adapter.

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

    // 1. Verify recipient account exists (NotFound -> Validation).
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

    account_free(tmp_account);

    // 2. Reject duplicate affiliate link.
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

    // 3. Build domain Affiliate (nickname validated).
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

    // 4. Persist affiliate.
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

// Join affiliate with account + user for AffiliateView.
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

bool affiliate_service_validate_send_target(
    AffiliateService *svc,
    UserId owner_user_id,
    AccountId from_account_id,
    AccountId recipient_account_id,
    ServiceError *err)
{
    if (!svc) {
        if (err) *err = service_error_validation(
            "AffiliateService::validate_send_target: invalid arguments");
        return false;
    }

    Account  *from_acc = NULL, *to_acc = NULL;
    RepoError rerr;

    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, from_account_id, &from_acc, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    if (!svc->account_repo.vtable->get_by_id(
            svc->account_repo.ctx, recipient_account_id, &to_acc, &rerr)) {
        account_free(from_acc);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (account_type_get(to_acc) != ACCOUNT_TYPE_REGULAR) {
        account_free(from_acc);
        account_free(to_acc);
        if (err) *err = service_error_validation("recipient must be a Regular account");
        return false;
    }

    if (account_currency(from_acc) != account_currency(to_acc)) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "This affiliate does not have a Regular %s account.",
                 currency_as_str(account_currency(from_acc)));
        account_free(from_acc);
        account_free(to_acc);
        if (err) *err = service_error_validation(msg);
        return false;
    }

    UserId recipient_user_id = account_user_id(to_acc);
    account_free(from_acc);
    account_free(to_acc);

    Affiliate **affiliates = NULL;
    size_t      aff_count  = 0;
    if (!svc->aff_repo.vtable->list_for_owner(
            svc->aff_repo.ctx, owner_user_id, &affiliates, &aff_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    bool found = false;
    for (size_t i = 0; i < aff_count; ++i) {
        AccountId saved_id = affiliate_recipient_sub_account_id(affiliates[i]);
        Account  *saved_acc = NULL;
        if (!svc->account_repo.vtable->get_by_id(
                svc->account_repo.ctx, saved_id, &saved_acc, &rerr)) {
            for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
            free(affiliates);
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (account_user_id(saved_acc).value == recipient_user_id.value) {
            found = true;
            account_free(saved_acc);
            break;
        }
        account_free(saved_acc);
    }

    for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
    free(affiliates);

    if (!found) {
        if (err) *err = service_error_validation("recipient is not one of your affiliates");
        return false;
    }

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

    // 1. Load the owner's affiliates.
    Affiliate **affiliates = NULL;
    size_t      aff_count  = 0;
    RepoError   rerr;

    if (!svc->aff_repo.vtable->list_for_owner(
            svc->aff_repo.ctx, owner_user_id, &affiliates, &aff_count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    // 2. Enrich each into an AffiliateView.
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

    size_t n = 0;
    if (params->for_send_currency_opt) {
        Currency want_curr;
        DomainError derr;
        if (!currency_from_str(params->for_send_currency_opt, &want_curr, &derr)) {
            for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
            free(affiliates);
            free(items);
            if (err) *err = service_error_validation("invalid currency");
            return false;
        }
        const char *want = currency_as_str(want_curr);

        char (*seen_nicknames)[64] = (char (*)[64])calloc(aff_count > 0 ? aff_count : 1, 64);
        if (!seen_nicknames) {
            for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
            free(affiliates);
            free(items);
            if (err) *err = service_error_internal("out of memory");
            return false;
        }
        size_t seen_count = 0;

        for (size_t i = 0; i < aff_count; ++i) {
            AffiliateView base;
            if (!build_affiliate_view(svc, affiliates[i], &base, err)) {
                for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
                free(affiliates);
                free(items);
                return false;
            }

            char nick_lower[64];
            str_to_lower(nick_lower, sizeof nick_lower, base.nickname);
            bool dup = false;
            for (size_t s = 0; s < seen_count; ++s) {
                if (strcmp(seen_nicknames[s], nick_lower) == 0) { dup = true; break; }
            }
            if (dup) continue;

            AccountId saved_id = { .value = base.recipient_sub_account_id };
            Account  *saved_acc = NULL;
            RepoError rerr;
            if (!svc->account_repo.vtable->get_by_id(
                    svc->account_repo.ctx, saved_id, &saved_acc, &rerr)) {
                for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
                free(affiliates);
                free(items);
                if (err) *err = service_error_from_repo(&rerr);
                return false;
            }

            Account **target_accs = NULL;
            size_t    target_n    = 0;
            if (!svc->account_repo.vtable->list_for_user(
                    svc->account_repo.ctx, account_user_id(saved_acc),
                    &target_accs, &target_n, &rerr)) {
                account_free(saved_acc);
                for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
                free(affiliates);
                free(items);
                if (err) *err = service_error_from_repo(&rerr);
                return false;
            }
            account_free(saved_acc);

            bool matched = false;
            for (size_t t = 0; t < target_n; ++t) {
                if (account_type_get(target_accs[t]) != ACCOUNT_TYPE_REGULAR) continue;
                if (account_currency(target_accs[t]) != want_curr) continue;
                items[n] = base;
                items[n].recipient_sub_account_id = account_id(target_accs[t]).value;
                strncpy(items[n].currency, want, sizeof items[n].currency - 1);
                items[n].currency[sizeof items[n].currency - 1] = '\0';
                strncpy(seen_nicknames[seen_count], nick_lower,
                        sizeof seen_nicknames[seen_count] - 1);
                seen_nicknames[seen_count][sizeof seen_nicknames[seen_count] - 1] = '\0';
                seen_count++;
                n++;
                matched = true;
                break;
            }
            for (size_t t = 0; t < target_n; ++t) account_free(target_accs[t]);
            free(target_accs);
            (void)matched;
        }
        free(seen_nicknames);
    } else {
        for (size_t i = 0; i < aff_count; ++i) {
            if (!build_affiliate_view(svc, affiliates[i], &items[i], err)) {
                for (size_t j = 0; j < aff_count; ++j) affiliate_free(affiliates[j]);
                free(affiliates);
                free(items);
                return false;
            }
        }
        n = aff_count;
    }
    for (size_t i = 0; i < aff_count; ++i) affiliate_free(affiliates[i]);
    free(affiliates);

    // 3. Apply search filter (min 2 chars).
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

    // 4. Apply currency filter.
    if (!params->for_send_currency_opt && params->currency_opt) {
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

    // 5. Sort by nickname.
    if (n > 1) qsort(items, n, sizeof(AffiliateView), compare_view_by_nickname);
    bool desc = params->sort_opt && strcmp(params->sort_opt, "desc") == 0;
    if (desc && n > 1) {
        for (size_t i = 0, j = n - 1; i < j; ++i, --j) {
            AffiliateView tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
        }
    }

    // 6. Paginate results.
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

    // 1. Resolve target user by tag.
    User     *target_user = NULL;
    RepoError rerr;
    if (!svc->user_repo.vtable->get_by_tag(
            svc->user_repo.ctx, tag_trim, &target_user, &rerr)) {
        if (rerr.code == REPO_ERROR_NOT_FOUND) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "No Gentlix user was found with tag \"%s\". Check the spelling and try again.",
                     tag_trim);
            if (err) *err = service_error_validation(msg);
        } else {
            if (err) *err = service_error_from_repo(&rerr);
        }
        return false;
    }

    UserId target_user_id = user_id(target_user);

    // 2. Load owner and target accounts.
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

    // 3. Match currencies owner can send to.
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
        if (account_type_get(target_accs[i]) != ACCOUNT_TYPE_REGULAR) {
            continue;
        }
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
            "This user has no Regular account in a currency you can send to. Transfers can only be made to Regular accounts.");
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