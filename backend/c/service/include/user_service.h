#ifndef C_USER_SERVICE_H
#define C_USER_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "ids.h"
#include "user.h"
#include "account.h"
#include "repo_error.h"
#include "service_error.h"
#include "db/include/account_repo.h"
#include "db/include/user_repo.h"

/*
 * Repository abstractions (ports) with explicit vtables.
 *
 * This mirrors the Rust traits:
 *
 *   trait UserRepository { ... }
 *   trait AccountRepository { ... }
 *
 * and the OOC-style Class+method table from the book.
 *
 * It is exactly what you will discuss in Chapter 3.3:
 * "Polymorphism and Dynamic Dispatch: vtables by Hand vs Traits".
 */

/* ---- UserRepository interface ----------------------------------------- */

typedef struct UserRepositoryVTable {
    bool (*get_by_id)(
        void *ctx,
        UserId id,
        User **out,
        RepoError *err);

    bool (*get_by_email)(
        void *ctx,
        const char *email_str,
        User **out,
        RepoError *err);

    bool (*get_by_tag)(
        void *ctx,
        const char *tag,
        User **out,
        RepoError *err);

    bool (*insert)(
        void *ctx,
        const User *user,
        UserId *out_id,
        RepoError *err);

    bool (*update)(
        void *ctx,
        const User *user,
        RepoError *err);

    bool (*delete_)(
        void *ctx,
        UserId id,
        RepoError *err);
} UserRepositoryVTable;

typedef struct UserRepository {
    const UserRepositoryVTable *vtable;
    void *ctx; /* concrete implementation, e.g. UserRepo* */
} UserRepository;

/* ---- AccountRepository interface (only what UserService needs) ------- */

typedef struct AccountRepositoryVTable {
    bool (*list_for_user)(
        void *ctx,
        UserId user_id,
        Account ***out_accounts,
        size_t *out_count,
        RepoError *err);

    bool (*exists_by_iban)(
        void *ctx,
        const char *iban_str,
        bool *out_exists,
        RepoError *err);

    bool (*insert)(
        void *ctx,
        const Account *account,
        AccountId *out_id,
        RepoError *err);
} AccountRepositoryVTable;

typedef struct AccountRepository {
    const AccountRepositoryVTable *vtable;
    void *ctx;
} AccountRepository;

/*
 * Concrete adapters from DB-layer repositories to these abstractions.
 *
 * These are your "ports and adapters" in C.
 */

UserRepository  user_repository_from_user_repo(UserRepo *repo);
AccountRepository account_repository_from_account_repo(AccountRepo *repo);

/* ---- DTOs / read models ---------------------------------------------- */

typedef struct RegisterUserCommand {
    const char   *tag;
    const char   *email;
    const char   *first_name;
    const char   *last_name;
    const char   *phone_opt;          /* NULL or string */
    const struct tm *birth_date_opt;  /* NULL or pointer */
    const char   *password_hash;      /* already hashed */
} RegisterUserCommand;

typedef struct {
    const char *email;
    const char *password;
} LoginUserCommand;

typedef struct {
    UserId user_id;
    char tag[64];
} LoginUserResult;

typedef struct UserWithAccounts {
    User     *user;         /* owned by caller, must user_free */
    Account **accounts;     /* array of Account*, may be NULL if count == 0 */
    size_t    account_count;
} UserWithAccounts;

/* ---- UserService "object" -------------------------------------------- */

typedef struct UserService UserService;

/* Constructor / destructor */

UserService *user_service_new(UserRepository user_repo,
                              AccountRepository account_repo);

void         user_service_free(UserService *svc);

/* Methods */

bool user_service_register_user(UserService *svc,
                                const RegisterUserCommand *cmd,
                                UserId *out_user_id,
                                ServiceError *err);

bool user_service_login_user(UserService *svc,
                             const LoginUserCommand *cmd,
                             LoginUserResult *out,
                             ServiceError *err);

bool user_service_get_user_with_accounts(UserService *svc,
                                         UserId user_id,
                                         UserWithAccounts *out,
                                         ServiceError *err);

bool user_service_delete_user(UserService *svc,
                              UserId user_id,
                              ServiceError *err);

#endif