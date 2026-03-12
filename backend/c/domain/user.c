#include "include/user.h"

#include <assert.h>
#include <ctype.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Concrete representation, hidden. */
struct User {
    bool has_id;
    UserId id;
    char tag[64];
    Email email;
    char first_name[64];
    char last_name[64];

    char phone[32];
    bool has_phone;

    struct tm birth_date;
    bool has_birth_date;

    char password_hash[128];
};

static bool normalize_required(
    const char *s,
    const char *field,
    char *out,
    size_t out_size,
    DomainError *err
) {
    if (!s) {
        if (err) *err = domain_error_validation("field is null");
        return false;
    }

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

static void normalize_optional(
    const char *s,
    char *out,
    size_t out_size,
    bool *has_value
) {
    if (!has_value) return;

    if (!s) {
        *has_value = false;
        return;
    }

    const char *start = s;
    while (*start && isspace((unsigned char)*start)) ++start;

    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) --end;

    size_t len = (size_t)(end - start);
    if (len == 0) {
        *has_value = false;
        return;
    }

    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    *has_value = true;
}

static bool user_init(
    bool has_id,
    UserId id,
    const char *tag,
    const Email *email,
    const char *first_name,
    const char *last_name,
    const char *phone_opt,
    const struct tm *birth_date_opt,
    const char *password_hash,
    User *out,
    DomainError *err
) {
    if (!out || !email) {
        if (err) *err = domain_error_validation("User: invalid arguments");
        return false;
    }

    User tmp;
    tmp.has_id = has_id;
    tmp.id = id;
    tmp.email = *email;

    if (!normalize_required(tag, "Tag", tmp.tag, sizeof(tmp.tag), err))
        return false;
    if (!normalize_required(first_name, "First name",
                            tmp.first_name, sizeof(tmp.first_name), err))
        return false;
    if (!normalize_required(last_name, "Last name",
                            tmp.last_name, sizeof(tmp.last_name), err))
        return false;
    if (!normalize_required(password_hash, "Password hash",
                            tmp.password_hash, sizeof(tmp.password_hash), err))
        return false;

    normalize_optional(phone_opt, tmp.phone, sizeof(tmp.phone),
                       &tmp.has_phone);

    if (birth_date_opt) {
        tmp.birth_date = *birth_date_opt;
        tmp.has_birth_date = true;
    } else {
        memset(&tmp.birth_date, 0, sizeof(tmp.birth_date));
        tmp.has_birth_date = false;
    }

    *out = tmp;
    return true;
}

static User *user_new_object(DomainError *err) {
    User *u = (User *)malloc(sizeof *u);
    if (!u) {
        if (err) *err = domain_error_validation("User: out of memory");
        return NULL;
    }
    return u;
}

/* Public API */

User *user_create(
    const char *tag,
    const Email *email,
    const char *first_name,
    const char *last_name,
    const char *phone_opt,
    const struct tm *birth_date_opt,
    const char *password_hash,
    DomainError *err
) {
    UserId dummy = (UserId){ 0 };
    User *u = user_new_object(err);
    if (!u) {
        return NULL;
    }

    if (!user_init(false, dummy, tag, email, first_name, last_name,
                   phone_opt, birth_date_opt, password_hash, u, err)) {
        free(u);
        return NULL;
    }

    return u;
}

User *user_rehydrate(
    UserId id,
    const char *tag,
    const Email *email,
    const char *first_name,
    const char *last_name,
    const char *phone_opt,
    const struct tm *birth_date_opt,
    const char *password_hash,
    DomainError *err
) {
    User *u = user_new_object(err);
    if (!u) {
        return NULL;
    }

    if (!user_init(true, id, tag, email, first_name, last_name,
                   phone_opt, birth_date_opt, password_hash, u, err)) {
        free(u);
        return NULL;
    }

    return u;
}

/* Getters */

bool user_has_id(const User *u) {
    assert(u);
    return u->has_id;
}

UserId user_id(const User *u) {
    assert(u);
    return u->id;
}

const char *user_tag(const User *u) {
    assert(u);
    return u->tag;
}

const Email *user_email(const User *u) {
    assert(u);
    return &u->email;
}

const char *user_first_name(const User *u) {
    assert(u);
    return u->first_name;
}

const char *user_last_name(const User *u) {
    assert(u);
    return u->last_name;
}

bool user_has_phone(const User *u) {
    assert(u);
    return u->has_phone;
}

const char *user_phone(const User *u) {
    assert(u);
    return u->has_phone ? u->phone : NULL;
}

bool user_has_birth_date(const User *u) {
    assert(u);
    return u->has_birth_date;
}

struct tm user_birth_date(const User *u) {
    assert(u);
    return u->birth_date;
}

const char *user_password_hash(const User *u) {
    assert(u);
    return u->password_hash;
}

/* Setters */

bool user_set_tag(User *u, const char *tag, DomainError *err) {
    assert(u);
    return normalize_required(tag, "Tag", u->tag, sizeof(u->tag), err);
}

void user_set_email(User *u, const Email *email) {
    if (!u || !email) return;
    u->email = *email;
}

bool user_set_first_name(User *u, const char *first_name, DomainError *err) {
    assert(u);
    return normalize_required(first_name, "First name",
                              u->first_name, sizeof(u->first_name), err);
}

bool user_set_last_name(User *u, const char *last_name, DomainError *err) {
    assert(u);
    return normalize_required(last_name, "Last name",
                              u->last_name, sizeof(u->last_name), err);
}

void user_set_phone(User *u, const char *phone_opt) {
    if (!u) return;
    normalize_optional(phone_opt, u->phone, sizeof(u->phone), &u->has_phone);
}

void user_set_birth_date(User *u, const struct tm *birth_date_opt) {
    if (!u) return;
    if (birth_date_opt) {
        u->birth_date = *birth_date_opt;
        u->has_birth_date = true;
    } else {
        memset(&u->birth_date, 0, sizeof(u->birth_date));
        u->has_birth_date = false;
    }
}

bool user_set_password_hash(User *u, const char *password_hash, DomainError *err) {
    assert(u);
    return normalize_required(password_hash, "Password hash",
                              u->password_hash, sizeof(u->password_hash), err);
}

/* Persistence hook */

void user_set_id_after_insert(User *u, UserId id) {
    if (!u) return;
    u->has_id = true;
    u->id = id;
}

/* Destructor */

void user_free(User *u) {
    free(u);
}
