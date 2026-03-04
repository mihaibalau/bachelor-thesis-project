#ifndef C_EMAIL_H
#define C_EMAIL_H

#include "error.h"
#include <stdbool.h>
#include <stddef.h>

#define EMAIL_MAX_LEN 62

typedef struct {
    char value[EMAIL_MAX_LEN + 1];
} Email;

const char *email_as_cstr(const Email *email);

/* Email::from_str / TryFrom<String> RUST */
bool email_try_create(const char *raw, Email *out, DomainError *err);

#endif