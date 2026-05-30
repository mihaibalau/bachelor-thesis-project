#include "include/account_service.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Adapter: AccountRepo -> AccountServiceRepository
 *
 * Each adapter function simply casts ctx back to AccountRepo* and
 * forwards the call.  This thin indirection is the "adapter" in the
 * ports-and-adapters pattern (Zero To Production, §9).
 *
 * §3.3: The vtable + ctx pair is structurally identical to a C++
 * vtable + this pointer — the book calls this pattern "Object with
 * methods", and it is exactly what Rust trait objects (dyn Trait) compile
 * down to under the hood.
 */

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

/*
 * Static vtable: one shared instance for all AccountRepo adapters.
 *
 * §3.3: Static vtables are the canonical way to implement
 * polymorphism in C without dynamic allocation of the dispatch table.
 */
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


/*  AccountService concrete type  (hidden — §3.1 Encapsulation)     */

/*
 * Rust equivalent:
 *
 *   pub struct AccountService<R: AccountRepository> {
 *       repo: Arc<R>,
 *   }
 *
 * §3.4 Composition: AccountService *has* a repository via value
 * embedding; there is no inheritance anywhere in this design.
 */
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

/*  Internal helpers                                                     */

/*
 * Translate a RepoError into a ServiceError at the service boundary.
 *
 * §4.2 Error Propagation: this is the C counterpart of Rust's
 *   `From<RepoError> for ServiceError` automatic conversion, plus the
 *   explicit match arm:
 *     Err(RepoError::NotFound(_)) => Err(ServiceError::not_found("account"))
 */
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

/*  Public methods                                                       */

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

    /*
     * 1. Parse IBAN using the domain type.
     *
     * §4.1 Type-Driven Invariants: we never reach the database with an
     * invalid IBAN.  The domain parse is the single source of truth.
     * Mirrors:
     *   let iban: IBAN = iban_str.parse().map_err(ServiceError::Domain)?;
     */
    IBAN iban;
    DomainError derr;
    if (!iban_try_create(cmd->iban_str, &iban, &derr)) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    /*
     * 2. Enforce at-most-one account of a given type for this user.
     *
     * §4.1: A duplicate maps to SERVICE_ERROR_CONFLICT so the API layer
     * can produce HTTP 409.
     * Mirrors:
     *   if self.repo.exists_by_account_type(user_id, account_type).await? {
     *       return Err(ServiceError::conflict("account", ...));
     *   }
     */
    RepoError rerr;
    bool exists = false;

    if (!svc->repo.vtable->exists_by_account_type(
            svc->repo.ctx,
            cmd->user_id,
            cmd->account_type,
            &exists,
            &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (exists) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "user already has an account of type '%s'",
                 account_type_as_str(cmd->account_type));
        if (err) *err = service_error_conflict("account", msg);
        return false;
    }

    /*
     * 3. Enforce global IBAN uniqueness.
     *
     * Mirrors:
     *   if self.repo.exists_by_iban(iban.as_str()).await? {
     *       return Err(ServiceError::conflict("account", ...));
     *   }
     */
    exists = false;
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

    /*
     * 4. Build the domain Account through its constructor.
     *
     * §3.1 / §4.1: account_create enforces balance >= 0 and
     * validates the IBAN, so we never insert a structurally invalid
     * entity into the database.
     * Mirrors:
     *   let account = Account::create(user_id, account_type,
     *                                 currency, initial_balance_cents, iban)?;
     */
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

    /* 5. Persist through the repository interface. */
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

    /* 2. Reject negative opening balance. */
    if (initial_balance_cents < 0) {
        if (err) *err = service_error_validation(
            "initial_balance_cents must be >= 0");
        return false;
    }

    /* 3. Enforce at-most-one account of a given type for this user. */
    RepoError rerr;
    bool exists = false;
    if (!svc->repo.vtable->exists_by_account_type(
            svc->repo.ctx, user_id, account_type, &exists, &rerr)) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }
    if (exists) {
        char msg[128];
        snprintf(msg, sizeof msg,
                 "user already has an account of type '%s'",
                 account_type_as_str(account_type));
        if (err) *err = service_error_conflict("account", msg);
        return false;
    }

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

        /* A NOT_FOUND here is impossible; treat anything else as a real error
         * unless we still have retries left (assume an IBAN clash). */
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

    /*
     * `open_account` enforces at-most-one account *per account type* for a user
     * (see exists_by_account_type), regardless of currency. Availability must
     * mirror that rule: once the user owns ANY account of a given type, every
     * currency for that type becomes unavailable.
     */
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
        if ((size_t)t < ACCOUNT_TYPE_COUNT) {
            for (size_t c = 0; c < CURRENCY_COUNT; ++c) {
                out->available[t][c] = false;
            }
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

    /*
     * §4.2 Error Propagation: translate REPO_ERROR_NOT_FOUND into
     * SERVICE_ERROR_NOT_FOUND at the service boundary so the caller
     * never sees a raw RepoError.
     *
     * Mirrors:
     *   match self.repo.get_by_id(account_id).await {
     *       Ok(account)               => Ok(account),
     *       Err(RepoError::NotFound(_))=> Err(ServiceError::not_found("account")),
     *       Err(e)                    => Err(ServiceError::from(e)),
     *   }
     */
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