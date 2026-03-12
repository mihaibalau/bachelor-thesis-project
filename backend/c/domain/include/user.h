#ifndef C_USER_H
#define C_USER_H

#include "error.h"
#include "ids.h"
#include "email.h"

#include <stdbool.h>
#include <time.h>

/* Opaque User type. */

typedef struct User User;

/* Constructors (heap-allocated) */

User *user_create(
    const char *tag,
    const Email *email,
    const char *first_name,
    const char *last_name,
    const char *phone_opt,          /* NULL or string */
    const struct tm *birth_date_opt,
    const char *password_hash,
    DomainError *err
);

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
);

/* Getters */

bool         user_has_id(const User *u);
UserId       user_id(const User *u);
const char  *user_tag(const User *u);
const Email *user_email(const User *u);
const char  *user_first_name(const User *u);
const char  *user_last_name(const User *u);
bool         user_has_phone(const User *u);
const char  *user_phone(const User *u); /* NULL if not present */
bool         user_has_birth_date(const User *u);
struct tm    user_birth_date(const User *u);
const char  *user_password_hash(const User *u);

/* Setters */

bool user_set_tag(User *u, const char *tag, DomainError *err);
void user_set_email(User *u, const Email *email);
bool user_set_first_name(User *u, const char *first_name, DomainError *err);
bool user_set_last_name(User *u, const char *last_name, DomainError *err);
void user_set_phone(User *u, const char *phone_opt);
void user_set_birth_date(User *u, const struct tm *birth_date_opt);
bool user_set_password_hash(User *u, const char *password_hash, DomainError *err);

/* Persistence hook */

void user_set_id_after_insert(User *u, UserId id);

/* Destructor */

void user_free(User *u);

#endif /* C_USER_H */
