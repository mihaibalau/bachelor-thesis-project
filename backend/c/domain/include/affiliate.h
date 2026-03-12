#ifndef C_AFFILIATE_H
#define C_AFFILIATE_H

#include "error.h"
#include "ids.h"

#include <stdbool.h>

/* Opaque Affiliate type. */

typedef struct Affiliate Affiliate;

/* Constructor */

Affiliate *affiliate_new(
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    DomainError *err
);

/* Getters */

UserId    affiliate_owner_user_id(const Affiliate *a);
AccountId affiliate_recipient_sub_account_id(const Affiliate *a);
const char *affiliate_nickname(const Affiliate *a);

/* Destructor */

void affiliate_free(Affiliate *a);

#endif /* C_AFFILIATE_H */
