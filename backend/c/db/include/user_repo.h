#ifndef C_USER_REPO_H
#define C_USER_REPO_H

#include <stddef.h>
#include <stdbool.h>

#include "db.h"
#include "repo_error.h"
#include "ids.h"
#include "user.h"   /* User opaque type */

typedef struct UserRepo UserRepo;

UserRepo *user_repo_new(Db *db);
void      user_repo_free(UserRepo *repo);

bool user_repo_get_by_id   (UserRepo *repo, UserId id,    User **out, RepoError *err);
bool user_repo_get_by_email(UserRepo *repo, const char *email_str, User **out, RepoError *err);
bool user_repo_get_by_tag  (UserRepo *repo, const char *tag,       User **out, RepoError *err);

bool user_repo_insert(UserRepo *repo, const User *user, UserId *out_id, RepoError *err);

bool user_repo_update(UserRepo *repo, const User *user, RepoError *err);
bool user_repo_delete(UserRepo *repo, UserId id, RepoError *err);

/* Transaction control on the underlying shared connection. */
bool user_repo_begin(UserRepo *repo, RepoError *err);
bool user_repo_commit(UserRepo *repo, RepoError *err);
bool user_repo_rollback(UserRepo *repo);

#endif //C_USER_REPO_H