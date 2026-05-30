#ifndef C_ACCOUNT_SERVICE_H
#define C_ACCOUNT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "ids.h"
#include "account.h"
#include "account_type.h"
#include "currency.h"
#include "repo_error.h"
#include "service_error.h"
#include "account_repo.h"

/*
 * Repository abstractions (ports) with explicit vtables.
 *
 * AccountRepositoryVTable mirrors the Rust trait:
 *
 *   pub trait AccountRepository: Send + Sync {
 *       async fn list_for_user(...) -> ...;
 *       async fn exists_by_account_type(...) -> ...;
 *       async fn exists_by_iban(...) -> ...;
 *       async fn get_by_id(...) -> ...;
 *       async fn insert(...) -> ...;
 *       async fn update(...) -> ...;
 *   }
 *
 */

/* ---- UserRepository interface ----------------------------------------- */

typedef struct AccountServiceRepositoryVTable {

    bool (*list_for_user)(
        void *ctx,
        UserId user_id,
        Account ***out_accounts,
        size_t *out_count,
        RepoError *err);

    bool (*exists_by_account_type)(
        void *ctx,
        UserId user_id,
        AccountType account_type,
        bool *out_exists,
        RepoError *err);

    bool (*exists_by_iban)(
        void *ctx,
        const char *iban_str,
        bool *out_exists,
        RepoError *err);

    bool (*get_by_id)(
        void *ctx,
        AccountId id,
        Account **out,
        RepoError *err);

    bool (*insert)(
        void *ctx,
        const Account *account,
        AccountId *out_id,
        RepoError *err);

} AccountServiceRepositoryVTable;

typedef struct AccountServiceRepository {
    const AccountServiceRepositoryVTable *vtable;
    void *ctx;
} AccountServiceRepository;

/*
 * Concrete adapters from DB-layer repositories to these abstractions.
 *
 * These are your "ports and adapters" in C.
 */

AccountServiceRepository account_service_repository_from_repo(AccountRepo *repo);

/* ---- DTOs / read models ---------------------------------------------- */

typedef struct OpenAccountCommand {
    UserId      user_id;
    AccountType account_type;
    Currency    currency;
    int64_t     initial_balance_cents;
    const char *iban_str;
} OpenAccountCommand;

/*
 * Availability matrix mirroring Rust's AccountAvailability
 * (HashMap<AccountType, Vec<Currency>>): available[type][currency] == true
 * means the user does NOT yet hold an account of that (type, currency) pair
 * and may therefore open one.
 */
typedef struct AccountAvailability {
    bool available[ACCOUNT_TYPE_COUNT][CURRENCY_COUNT];
} AccountAvailability;

/* ---- AccountService "object" ---------------------------------------- */

typedef struct AccountService AccountService;

/* Constructor / Destructor */

AccountService *account_service_new(AccountServiceRepository repo);
void            account_service_free(AccountService *svc);

/* Methods */

/*
 * open_account: open a new bank account for a user.
 *
 * Steps (matching Rust open_account exactly):
 *   1. Parse and validate IBAN via domain type.
 *   2. Enforce at-most-one account per (user, type).
 *   3. Enforce global IBAN uniqueness.
 *   4. Build domain Account via constructor.
 *   5. Persist through the repository interface.
 *
 * Returns true + sets *out_id on success.
 * Returns false + sets *err on any validation, conflict, or repo failure.
 */
bool account_service_open_account(
    AccountService *svc,
    const OpenAccountCommand *cmd,
    AccountId *out_id,
    ServiceError *err
);

/*
 * open_account_raw: accept raw strings from the API and perform all parsing,
 * validation and IBAN generation here in the service (the server layer only
 * forwards the request body).
 *
 * Steps (matching Rust open_account_raw + open_account exactly):
 *   1. Parse account_type / currency from strings (validation error on fail).
 *   2. Reject negative initial_balance_cents.
 *   3. Enforce at-most-one account per (user, type).
 *   4. Generate a unique IBAN, build the domain Account, and persist it
 *      (retrying with a fresh IBAN on collision).
 *
 * Mirrors:
 *   pub async fn open_account_raw(&self, user_id, account_type_str,
 *       currency_str, initial_balance_cents) -> ServiceResult<AccountId>
 */
bool account_service_open_account_raw(
    AccountService *svc,
    UserId user_id,
    const char *account_type_str,
    const char *currency_str,
    int64_t initial_balance_cents,
    AccountId *out_id,
    ServiceError *err
);

/*
 * get_account_availability: for every (AccountType, Currency) pair, report
 * whether the user may still open such an account (i.e. does not already
 * hold one).
 *
 * Mirrors:
 *   pub async fn get_account_availability(&self, user_id)
 *       -> ServiceResult<AccountAvailability>
 */
bool account_service_get_account_availability(
    AccountService *svc,
    UserId user_id,
    AccountAvailability *out,
    ServiceError *err
);

/*
 * get_account: load a single account by id.
 *
 * Maps REPO_ERROR_NOT_FOUND -> SERVICE_ERROR_NOT_FOUND at the service
 * boundary so callers never see a raw RepoError.
 *
 * Mirrors:
 *   pub async fn get_account(&self, account_id: AccountId)
 *       -> ServiceResult<Account>
 */
bool account_service_get_account(
    AccountService *svc,
    AccountId account_id,
    Account **out,
    ServiceError *err
);

#endif //C_ACCOUNT_SERVICE_H