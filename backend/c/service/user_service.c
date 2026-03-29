#include "include/user_service.h"

#include <stdlib.h>

#include "account.h"
#include "account_repo.h"
#include "email.h"
#include "error.h"
#include "ids.h"
#include "repo_error.h"
#include "user.h"
#include "user_repo.h"
#include "include/service_error.h"

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
    void *ctx,
    UserId user_id,
    Account ***out_accounts,
    size_t *out_count,
    RepoError *err
) {
    return account_repo_list_for_user(
        (AccountRepo *)ctx,
        user_id,
        out_accounts,
        out_count,
        err
    );
}

static const AccountRepositoryVTable ACCOUNT_REPO_VTABLE = {
    account_repo_list_for_user_adapter
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

    /* 4. Construct the domain User. All heavy validation happens here. */

    User *user = user_create(
        cmd->tag,
        &email,
        cmd->first_name,
        cmd->last_name,
        cmd->phone_opt,
        cmd->birth_date_opt,
        cmd->password_hash,
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

    if (err) *err = service_error_ok();
    *out_user_id = new_id;
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