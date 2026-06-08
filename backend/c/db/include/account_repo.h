#ifndef C_ACCOUNT_REPO_H
#define C_ACCOUNT_REPO_H

#include <stddef.h>
#include <stdbool.h>

#include "db.h"
#include "repo_error.h"
#include "ids.h"
#include "account.h"   /* Account opaque type */

typedef struct AccountRepo AccountRepo;

AccountRepo *account_repo_new(Db *db);
void         account_repo_free(AccountRepo *repo);

/* Caller takes ownership of *out (must call account_free). */
bool account_repo_get_by_id(AccountRepo *repo, AccountId id, Account **out, RepoError *err);
bool account_repo_get_by_iban(AccountRepo *repo, const char *iban_str, Account **out, RepoError *err);

/* Allocates an array of Account*.
 * Caller must free each Account* with account_free, then free(*out_accounts).
 */
bool account_repo_list_for_user(
    AccountRepo *repo,
    UserId user_id,
    Account ***out_accounts,
    size_t *out_count,
    RepoError *err
);

/* Insert returns newly generated AccountId. */
bool account_repo_insert(AccountRepo *repo, const Account *account, AccountId *out_id, RepoError *err);

/* Update / delete by id, with not-found detection. */
bool account_repo_update(AccountRepo *repo, const Account *account, RepoError *err);
bool account_repo_delete(AccountRepo *repo, AccountId id, RepoError *err);

bool account_repo_exists_by_iban(AccountRepo *repo, const char *iban_str, bool *out_exists, RepoError *err);
bool account_repo_exists_by_account_type(AccountRepo *repo, UserId user_id, AccountType account_type, bool *out_exists, RepoError *err);
bool account_repo_exists_by_type_and_currency(AccountRepo *repo, UserId user_id, AccountType account_type, Currency currency, bool *out_exists, RepoError *err);

#endif //C_ACCOUNT_REPO_H