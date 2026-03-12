#ifndef C_AFFILIATE_REPO_H
#define C_AFFILIATE_REPO_H

#include <stddef.h>
#include <stdbool.h>

#include "db.h"
#include "repo_error.h"
#include "ids.h"
#include "affiliate.h"   /* Affiliate opaque type */

typedef struct AffiliateRepo AffiliateRepo;

AffiliateRepo *affiliate_repo_new(Db *db);
void           affiliate_repo_free(AffiliateRepo *repo);

/* Get single affiliate by (owner_user_id, recipient_sub_account_id). */
bool affiliate_repo_get(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    Affiliate **out,
    RepoError *err
);

bool affiliate_repo_list_for_owner(
    AffiliateRepo *repo,
    UserId owner_user_id,
    Affiliate ***out_affiliates,
    size_t *out_count,
    RepoError *err
);

bool affiliate_repo_insert(
    AffiliateRepo *repo,
    const Affiliate *affiliate,
    RepoError *err
);

bool affiliate_repo_update_nickname(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    RepoError *err
);

/* Delete & exists. */
bool affiliate_repo_delete(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    RepoError *err
);

bool affiliate_repo_exists(
    AffiliateRepo *repo,
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    bool *out_exists,
    RepoError *err
);

#endif /* C_AFFILIATE_REPO_H */