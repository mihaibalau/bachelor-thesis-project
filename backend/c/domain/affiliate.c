#include "include/affiliate.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool normalize_required(const char *s, const char *field,
                               char *out, size_t out_size,
                               DomainError *err) {
    if (!s) {
        if (err) *err = domain_error_validation("field is null");
        return false;
    }

    /* trim */
    const char *start = s;
    while (*start && isspace((unsigned char)*start)) ++start;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) --end;

    size_t len = (size_t)(end - start);
    if (len == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s must not be empty", field);
        if (err) *err = domain_error_validation(msg);
        return false;
    }
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    if (err) *err = domain_error_ok();
    return true;
}

bool affiliate_new(
    UserId owner_user_id,
    AccountId recipient_sub_account_id,
    const char *nickname,
    Affiliate *out,
    DomainError *err
) {
    if (!out) {
        if (err) *err = domain_error_validation("Affiliate is null");
        return false;
    }

    Affiliate tmp;
    tmp.owner_user_id = owner_user_id;
    tmp.recipient_sub_account_id = recipient_sub_account_id;

    if (!normalize_required(nickname, "nickname",
                            tmp.nickname, sizeof(tmp.nickname), err)) {
        return false;
                            }

    *out = tmp;
    return true;
}

UserId affiliate_owner_user_id(const Affiliate *a) { return a->owner_user_id; }
AccountId affiliate_recipient_sub_account_id(const Affiliate *a) { return a->recipient_sub_account_id; }
const char *affiliate_nickname(const Affiliate *a) { return a->nickname; }
