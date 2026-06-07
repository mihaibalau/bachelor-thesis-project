#include "include/account_service.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// AccountRepo -> AccountServiceRepository adapter (vtable + ctx).

static bool acct_repo_list_for_user_adapter(
    void *ctx,
    UserId user_id,
    Account ***out_accounts,
    size_t *out_count,
    RepoError *err
) {
    return account_repo_list_for_user(
        (AccountRepo *)ctx, user_id, out_accounts, out_count, err);
}

static bool acct_repo_exists_by_account_type_adapter(
    void *ctx,
    UserId user_id,
    AccountType account_type,
    bool *out_exists,
    RepoError *err
) {
    return account_repo_exists_by_account_type(
        (AccountRepo *)ctx, user_id, account_type, out_exists, err);
}

static bool acct_repo_exists_by_iban_adapter(
    void *ctx,
    const char *iban_str,
    bool *out_exists,
    RepoError *err
) {
    return account_repo_exists_by_iban(
        (AccountRepo *)ctx, iban_str, out_exists, err);
}

static bool acct_repo_get_by_id_adapter(
    void *ctx,
    AccountId id,
    Account **out,
    RepoError *err
) {
    return account_repo_get_by_id(
        (AccountRepo *)ctx, id, out, err);
}

static bool acct_repo_insert_adapter(
    void *ctx,
    const Account *account,
    AccountId *out_id,
    RepoError *err
) {
    return account_repo_insert(
        (AccountRepo *)ctx, account, out_id, err);
}

static bool acct_repo_update_adapter(
    void *ctx,
    const Account *account,
    RepoError *err
) {
    return account_repo_update(
        (AccountRepo *)ctx, account, err);
}

static const AccountServiceRepositoryVTable ACCOUNT_REPO_VTABLE = {
    acct_repo_list_for_user_adapter,
    acct_repo_exists_by_account_type_adapter,
    acct_repo_exists_by_iban_adapter,
    acct_repo_get_by_id_adapter,
    acct_repo_insert_adapter
};

AccountServiceRepository account_service_repository_from_repo(AccountRepo *repo) {
    AccountServiceRepository r;
    r.vtable = &ACCOUNT_REPO_VTABLE;
    r.ctx    = repo;
    return r;
}


struct AccountService {
    AccountServiceRepository repo;
};

AccountService *account_service_new(AccountServiceRepository repo) {
    AccountService *svc = (AccountService *)malloc(sizeof *svc);
    if (!svc) return NULL;
    svc->repo = repo;
    return svc;
}

void account_service_free(AccountService *svc) {
    free(svc);
}

// Map RepoError -> ServiceError at service boundary.
static ServiceError translate_repo_error(const RepoError *rerr,
                                          const char *entity) {
    if (!rerr || rerr->code == REPO_ERROR_NONE) {
        return service_error_ok();
    }
    if (rerr->code == REPO_ERROR_NOT_FOUND) {
        return service_error_not_found(entity);
    }
    return service_error_from_repo(rerr);
}

bool account_service_open_account(
    AccountService *svc,
    const OpenAccountCommand *cmd,
    AccountId *out_id,
    ServiceError *err
) {
    if (!svc || !cmd || !out_id) {
        if (err) *err = service_error_validation(
            "AccountService::open_account: invalid arguments");
        return false;
    }

    // 1. Parse IBAN via domain type.
    IBAN iban;
    DomainError derr;
    if (!iban_try_create(cmd->iban_str, &iban, &derr)) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    // 2. Enforce at-most-one account per (type, currency).
    RepoError rerr;
    Account **owned = NULL;
    size_t    owned_n = 0;

    if (!svc->repo.vtable->list_for_user(
            svc->repo.ctx, cmd->user_id, &owned, &owned_n, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    for (size_t i = 0; i < owned_n; ++i) {
        bool same_type = account_type_get(owned[i]) == cmd->account_type;
        bool same_curr = account_currency(owned[i]) == cmd->currency;
        account_free(owned[i]);
        if (same_type && same_curr) {
            free(owned);
            char msg[128];
            snprintf(msg, sizeof msg,
                     "you already have a %s %s account",
                     account_type_as_str(cmd->account_type),
                     currency_as_str(cmd->currency));
            if (err) *err = service_error_conflict("account", msg);
            return false;
        }
    }
    free(owned);

    // 3. Enforce global IBAN uniqueness.
    bool exists = false;
    if (!svc->repo.vtable->exists_by_iban(
            svc->repo.ctx,
            cmd->iban_str,
            &exists,
            &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (exists) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "IBAN '%s' is already in use", cmd->iban_str);
        if (err) *err = service_error_conflict("account", msg);
        return false;
    }

    // 4. Build domain Account and persist.
    Account *account = account_create(
        cmd->user_id,
        cmd->account_type,
        cmd->currency,
        cmd->initial_balance_cents,
        &iban,
        &derr
    );
    if (!account) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    AccountId new_id;
    if (!svc->repo.vtable->insert(
            svc->repo.ctx,
            account,
            &new_id,
            &rerr)) {
        account_free(account);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    account_free(account);

    *out_id = new_id;
    if (err) *err = service_error_ok();
    return true;
}

bool account_service_open_account_raw(
    AccountService *svc,
    UserId user_id,
    const char *account_type_str,
    const char *currency_str,
    int64_t initial_balance_cents,
    AccountId *out_id,
    ServiceError *err
) {
    if (!svc || !out_id) {
        if (err) *err = service_error_validation(
            "AccountService::open_account_raw: invalid arguments");
        return false;
    }


    // 1. Parse the raw strings into domain value objects.
    AccountType account_type;
    Currency    currency;
    DomainError derr;

    if (!account_type_from_str(account_type_str, &account_type, &derr)) {
        if (err) *err = service_error_validation("invalid account_type");
        return false;
    }
    if (!currency_from_str(currency_str, &currency, &derr)) {
        if (err) *err = service_error_validation("invalid currency");
        return false;
    }

    // 2. Reject negative opening balance.
    if (initial_balance_cents < 0) {
        if (err) *err = service_error_validation(
            "initial_balance_cents must be >= 0");
        return false;
    }

    // 3. Enforce at-most-one account per (type, currency).
    RepoError rerr;
    Account **owned = NULL;
    size_t    owned_n = 0;
    if (!svc->repo.vtable->list_for_user(
            svc->repo.ctx, user_id, &owned, &owned_n, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    for (size_t i = 0; i < owned_n; ++i) {
        bool same_type = account_type_get(owned[i]) == account_type;
        bool same_curr = account_currency(owned[i]) == currency;
        account_free(owned[i]);
        if (same_type && same_curr) {
            free(owned);
            char msg[128];
            snprintf(msg, sizeof msg,
                     "you already have a %s %s account",
                     account_type_as_str(account_type),
                     currency_as_str(currency));
            if (err) *err = service_error_conflict("account", msg);
            return false;
        }
    }
    free(owned);

    // 4. Generate a unique IBAN, build the Account and persist it.
    const int MAX_RETRIES = 5;
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        IBAN iban;
        if (!iban_generate(&iban, &derr)) {
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }

        bool iban_taken = false;
        if (!svc->repo.vtable->exists_by_iban(
                svc->repo.ctx, iban.value, &iban_taken, &rerr)) {
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
        if (iban_taken) {
            continue;
        }

        Account *account = account_create(
            user_id, account_type, currency, initial_balance_cents, &iban, &derr);
        if (!account) {
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }

        AccountId new_id;
        bool ok = svc->repo.vtable->insert(svc->repo.ctx, account, &new_id, &rerr);
        account_free(account);

        if (ok) {
            *out_id = new_id;
            if (err) *err = service_error_ok();
            return true;
        }

        // Retry on assumed IBAN clash; fail after max attempts.
        if (attempt + 1 >= MAX_RETRIES) {
            if (err) *err = service_error_from_repo(&rerr);
            return false;
        }
    }

    if (err) *err = service_error_conflict(
        "account", "unable to allocate a unique IBAN after retries");
    return false;
}

bool account_service_get_account_availability(
    AccountService *svc,
    UserId user_id,
    AccountAvailability *out,
    ServiceError *err
) {
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "AccountService::get_account_availability: invalid arguments");
        return false;
    }

    // Start with everything available, then subtract what the user holds.
    for (size_t t = 0; t < ACCOUNT_TYPE_COUNT; ++t)
        for (size_t c = 0; c < CURRENCY_COUNT; ++c)
            out->available[t][c] = true;

    // 2. Mark owned (type, currency) pairs unavailable.
    Account **accounts = NULL;
    size_t    count    = 0;
    RepoError rerr;

    if (!svc->repo.vtable->list_for_user(
            svc->repo.ctx, user_id, &accounts, &count, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        AccountType t = account_type_get(accounts[i]);
        Currency    c = account_currency(accounts[i]);
        if ((size_t)t < ACCOUNT_TYPE_COUNT && (size_t)c < CURRENCY_COUNT) {
            out->available[t][c] = false;
        }
        account_free(accounts[i]);
    }
    free(accounts);

    if (err) *err = service_error_ok();
    return true;
}

bool account_service_get_account(
    AccountService *svc,
    AccountId account_id,
    Account **out,
    ServiceError *err
) {
    if (!svc || !out) {
        if (err) *err = service_error_validation(
            "AccountService::get_account: invalid arguments");
        return false;
    }

    RepoError rerr;
    bool ok = svc->repo.vtable->get_by_id(
        svc->repo.ctx,
        account_id,
        out,
        &rerr
    );

    if (!ok) {
        if (err) *err = translate_repo_error(&rerr, "account");
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}