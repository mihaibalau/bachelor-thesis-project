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