#include "include/user_service.h"

#include <stdlib.h>
#include <string.h>
#include <argon2.h>
#include <openssl/rand.h>
#include "account.h"
#include "account_repo.h"
#include "email.h"
#include "error.h"
#include "ids.h"
#include "repo_error.h"
#include "user.h"
#include "user_repo.h"
#include "include/service_error.h"

/* ── Password hashing (Argon2id via libargon2) ───────────────────────────── */
/*
 * Mirrors the Rust UserService::register_user, which hashes the password
 * *inside the service* (on a blocking thread) rather than in the HTTP layer.
 * Keeping hashing here means the server layer never performs cryptographic /
 * business computation — it only (de)serializes and routes.
 *
 * Format: PHC encoded string "$argon2id$v=19$m=...,t=...,p=...$salt$hash",
 * exactly what the Rust `argon2` crate produces and what login verifies.
 */

#define ARGON_T_COST       3            /* time cost (iterations)      */
#define ARGON_M_COST       (64 * 1024)  /* memory cost in KiB (64 MiB) */
#define ARGON_PARALLELISM  1            /* lanes                       */
#define ARGON_HASH_LEN     32           /* bytes of hash               */
#define ARGON_SALT_LEN     16           /* bytes of salt               */
#define ARGON_ENCODED_LEN  128

static bool hash_password(const char *password, char out_encoded[ARGON_ENCODED_LEN]) {
    uint8_t salt[ARGON_SALT_LEN];

    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return false;
    }

    size_t encoded_len = argon2_encodedlen(
        ARGON_T_COST,
        ARGON_M_COST,
        ARGON_PARALLELISM,
        ARGON_SALT_LEN,
        ARGON_HASH_LEN,
        Argon2_id
    );
    if (encoded_len + 1 > ARGON_ENCODED_LEN) {
        return false;
    }

    int rc = argon2id_hash_encoded(
        ARGON_T_COST,
        ARGON_M_COST,
        ARGON_PARALLELISM,
        password,
        (uint32_t)strlen(password),
        salt,
        sizeof(salt),
        ARGON_HASH_LEN,
        out_encoded,
        ARGON_ENCODED_LEN
    );

    return rc == ARGON2_OK;
}

/* UserRepo adapter */

static bool user_repo_get_by_id_adapter(
    void *ctx,
    UserId id,
    User **out,
    RepoError *err
) {
    return user_repo_get_by_id((UserRepo *)ctx, id, out, err);
}

static bool user_repo_get_by_email_adapter(
    void *ctx,
    const char *email_str,
    User **out,
    RepoError *err
) {
    return user_repo_get_by_email((UserRepo *)ctx, email_str, out, err);
}

static bool user_repo_get_by_tag_adapter(
    void *ctx,
    const char *tag,
    User **out,
    RepoError *err
) {
    return user_repo_get_by_tag((UserRepo *)ctx, tag, out, err);
}

static bool user_repo_insert_adapter(
    void *ctx,
    const User *user,
    UserId *out_id,
    RepoError *err
) {
    return user_repo_insert((UserRepo *)ctx, user, out_id, err);
}

static bool user_repo_update_adapter(
    void *ctx,
    const User *user,
    RepoError *err
) {
    return user_repo_update((UserRepo *)ctx, user, err);
}

static bool user_repo_delete_adapter(
    void *ctx,
    UserId id,
    RepoError *err
) {
    return user_repo_delete((UserRepo *)ctx, id, err);
}

static const UserRepositoryVTable USER_REPO_VTABLE = {
    user_repo_get_by_id_adapter,
    user_repo_get_by_email_adapter,
    user_repo_get_by_tag_adapter,
    user_repo_insert_adapter,
    user_repo_update_adapter,
    user_repo_delete_adapter
};

UserRepository user_repository_from_user_repo(UserRepo *repo) {
    UserRepository r;
    r.vtable = &USER_REPO_VTABLE;
    r.ctx = repo;
    return r;
}

/* AccountRepo adapter (only list_for_user is needed by UserService) */

static bool account_repo_list_for_user_adapter(
    void *ctx, UserId user_id,
    Account ***out_accounts, size_t *out_count, RepoError *err
) {
    return account_repo_list_for_user(
        (AccountRepo *)ctx, user_id, out_accounts, out_count, err);
}

static bool account_repo_exists_by_iban_adapter(
    void *ctx, const char *iban_str, bool *out_exists, RepoError *err
) {
    return account_repo_exists_by_iban(
        (AccountRepo *)ctx, iban_str, out_exists, err);
}

static bool account_repo_insert_adapter(
    void *ctx, const Account *account, AccountId *out_id, RepoError *err
) {
    return account_repo_insert(
        (AccountRepo *)ctx, account, out_id, err);
}

static const AccountRepositoryVTable ACCOUNT_REPO_VTABLE = {
    account_repo_list_for_user_adapter,
    account_repo_exists_by_iban_adapter,
    account_repo_insert_adapter
};

AccountRepository account_repository_from_account_repo(AccountRepo *repo) {
    AccountRepository r;
    r.vtable = &ACCOUNT_REPO_VTABLE;
    r.ctx = repo;
    return r;
}

/*  UserService implementation                                           */

struct UserService {
    UserRepository    user_repo;
    AccountRepository account_repo;
};

UserService *user_service_new(UserRepository user_repo,
                              AccountRepository account_repo) {
    UserService *svc = (UserService *)malloc(sizeof *svc);
    if (!svc) {
        return NULL;
    }
    svc->user_repo    = user_repo;
    svc->account_repo = account_repo;
    return svc;
}

void user_service_free(UserService *svc) {
    free(svc);
}

/* Internal helper: map RepoError -> ServiceError and free optional User* */

static bool handle_repo_lookup_result(
    bool repo_ok,
    const RepoError *rerr,
    const char *entity,
    User **maybe_user,
    ServiceError *serr
) {
    if (repo_ok) {
        /* Found entity; caller will use the User* and then free it. */
        if (serr) *serr = service_error_ok();
        return true;
    }

    if (maybe_user && *maybe_user) {
        user_free(*maybe_user);
        *maybe_user = NULL;
    }

    if (!rerr) {
        if (serr) *serr = service_error_validation("Repository error without details");
        return false;
    }

    if (rerr->code == REPO_ERROR_NOT_FOUND) {
        if (serr) *serr = service_error_not_found(entity);
        return false;
    }

    if (serr) *serr = service_error_from_repo(rerr);
    return false;
}

/* --------------------------------------------------------------------- */
/*  Public methods                                                       */
/* --------------------------------------------------------------------- */

bool user_service_register_user(UserService *svc,
                                const RegisterUserCommand *cmd,
                                UserId *out_user_id,
                                ServiceError *err) {
    if (!svc || !cmd || !out_user_id) {
        if (err) *err = service_error_validation("UserService::register_user: invalid arguments");
        return false;
    }

    /* 1. Validate email format using the domain type. */
    Email email;
    DomainError derr;

    if (!email_try_create(cmd->email, &email, &derr)) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    /* 2. Check uniqueness for email. */

    User *tmp_user = NULL;
    RepoError rerr;

    bool repo_ok = svc->user_repo.vtable->get_by_email(
        svc->user_repo.ctx,
        cmd->email,
        &tmp_user,
        &rerr
    );

    if (repo_ok) {
        /* Found an existing user: conflict. */
        user_free(tmp_user);
        if (err) *err = service_error_conflict(
            "user",
            "email is already in use"
        );
        return false;
    }

    if (rerr.code != REPO_ERROR_NOT_FOUND) {
        /* Real error, not "not found". */
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* 3. Check uniqueness for tag. */

    tmp_user = NULL;
    repo_ok = svc->user_repo.vtable->get_by_tag(
        svc->user_repo.ctx,
        cmd->tag,
        &tmp_user,
        &rerr
    );

    if (repo_ok) {
        user_free(tmp_user);
        if (err) *err = service_error_conflict(
            "user",
            "tag is already in use"
        );
        return false;
    }

    if (rerr.code != REPO_ERROR_NOT_FOUND) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    /* 4. Hash the password (Argon2id) — business logic kept in the service,
     *    mirroring Rust's register_user which hashes via spawn_blocking. */
    if (!cmd->password) {
        if (err) *err = service_error_validation("password is required");
        return false;
    }

    char password_hash[ARGON_ENCODED_LEN];
    if (!hash_password(cmd->password, password_hash)) {
        if (err) *err = service_error_internal("password hashing failed");
        return false;
    }

    /* 5. Construct the domain User. All heavy validation happens here. */

    User *user = user_create(
        cmd->tag,
        &email,
        cmd->first_name,
        cmd->last_name,
        cmd->phone_opt,
        cmd->birth_date_opt,
        password_hash,
        &derr
    );
    if (!user) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    /* 5. Persist the user through the repository. */

    UserId new_id;
    if (!svc->user_repo.vtable->insert(
            svc->user_repo.ctx,
            user,
            &new_id,
            &rerr
        )) {
        user_free(user);
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    user_free(user);
    *out_user_id = new_id;

    /* 6. Create a default account with the new user account + IBAN for it */
    IBAN default_iban;
    bool iban_exists = false;

    if (!iban_generate(&default_iban, &derr)) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    for (;;) {
        RepoError rerr_iban;
        if (!svc->account_repo.vtable->exists_by_iban(
                svc->account_repo.ctx,
                default_iban.value,
                &iban_exists,
                &rerr_iban)) {
            if (err) *err = service_error_from_repo(&rerr_iban);
            return false;
                }
        if (!iban_exists) break;
        if (!iban_generate(&default_iban, &derr)) {
            if (err) *err = service_error_from_domain(&derr);
            return false;
        }
    }

    Account *default_account = account_create(
        new_id,
        ACCOUNT_TYPE_REGULAR,
        CURRENCY_RON,
        0,
        &default_iban,
        &derr
    );
    if (!default_account) {
        if (err) *err = service_error_from_domain(&derr);
        return false;
    }

    AccountId account_id;
    RepoError rerr_acc2;
    if (!svc->account_repo.vtable->insert(
            svc->account_repo.ctx,
            default_account,
            &account_id,
            &rerr_acc2)) {
        account_free(default_account);
        if (err) *err = service_error_from_repo(&rerr_acc2);
        return false;
            }

    account_free(default_account);

    if (err) *err = service_error_ok();
    return true;
}

bool user_service_login_user(UserService *svc,
                             const LoginUserCommand *cmd,
                             LoginUserResult *out,
                             ServiceError *err) {
    if (!svc || !cmd || !out) {
        if (err) *err = service_error_validation("UserService::login_user: invalid arguments");
        return false;
    }

    /* 1. Validate the email */
    Email email;
    DomainError derr;

    if (!email_try_create(cmd->email, &email, &derr)) {
        if (err) *err = service_error_validation("invalid credentials");
        return false;
    }

    /* 2. Find the user by the email */
    User *user = NULL;
    RepoError rerr;

    bool repo_ok = svc->user_repo.vtable->get_by_email(
        svc->user_repo.ctx,
        cmd->email,
        &user,
        &rerr
    );

    if (!repo_ok) {
        if (user) {
            user_free(user);
        }
        if (err) *err = service_error_validation("invalid credentials");
        return false;
    }

    /* 3. Check the password */
    const char *stored_hash = user_password_hash(user);

    int rc = argon2id_verify(
        stored_hash,
        cmd->password,
        (uint32_t)strlen(cmd->password)
    );

    if (rc != ARGON2_OK) {
        user_free(user);
        if (err) *err = service_error_validation("invalid credentials");
        return false;
    }

    /* 4. Password match → return UserId. */
    UserId uid = user_id(user);

    strncpy(out->tag, user_tag(user), sizeof(out->tag) - 1);
    out->tag[sizeof(out->tag) - 1] = '\0';

    user_free(user);

    out->user_id = uid;
    if (err) *err = service_error_ok();
    return true;
}

bool user_service_get_user_with_accounts(UserService *svc,
                                         UserId user_id,
                                         UserWithAccounts *out,
                                         ServiceError *err) {
    if (!svc || !out) {
        if (err) *err = service_error_validation("UserService::get_user_with_accounts: invalid arguments");
        return false;
    }

    /* 1. Load user. */

    User *user = NULL;
    RepoError rerr_user;

    bool ok = svc->user_repo.vtable->get_by_id(
        svc->user_repo.ctx,
        user_id,
        &user,
        &rerr_user
    );

    if (!ok) {
        if (rerr_user.code == REPO_ERROR_NOT_FOUND) {
            if (err) *err = service_error_not_found("user");
        } else if (err) {
            *err = service_error_from_repo(&rerr_user);
        }
        return false;
    }

    /* 2. Load accounts for user. */

    Account **accounts = NULL;
    size_t   account_count = 0;
    RepoError rerr_acc;

    ok = svc->account_repo.vtable->list_for_user(
        svc->account_repo.ctx,
        user_id,
        &accounts,
        &account_count,
        &rerr_acc
    );

    if (!ok) {
        user_free(user);
        if (err) *err = service_error_from_repo(&rerr_acc);
        return false;
    }

    /* 3. Success: populate DTO. Caller owns user + accounts[]. */

    out->user = user;
    out->accounts = accounts;
    out->account_count = account_count;

    if (err) *err = service_error_ok();
    return true;
}

bool user_service_delete_user(UserService *svc,
                              UserId user_id,
                              ServiceError *err) {
    if (!svc) {
        if (err) *err = service_error_validation("UserService::delete_user: invalid arguments");
        return false;
    }

    RepoError rerr;
    bool ok = svc->user_repo.vtable->delete_(
        svc->user_repo.ctx,
        user_id,
        &rerr
    );

    if (!ok) {
        if (err) *err = service_error_from_repo(&rerr);
        return false;
    }

    if (err) *err = service_error_ok();
    return true;
}