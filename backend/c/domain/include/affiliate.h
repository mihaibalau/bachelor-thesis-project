#ifndef C_AFFILIATE_H
#define C_AFFILIATE_H

#include "error.h"
#include "ids.h"
#include <stdbool.h>

typedef struct {
    UserId owner_user_id;
    AccountId recipient_sub_account_id;
    char nickname[64];
} Affiliate;

bool affiliate_new(
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    Affiliate *out,
    DomainError *err
);

UserId   affiliate_owner_user_id(const Affiliate *a);
AccountId affiliate_recipient_sub_account_id(const Affiliate *a);
const char *affiliate_nickname(const Affiliate *a);

#endif