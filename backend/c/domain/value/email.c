#include "../include/email.h"
#include <string.h>

const char *email_as_cstr(const Email *email) {
    return email ? email->value : "";
}

bool email_try_create(const char *raw, Email *out, DomainError *err) {
    if (!raw || !out) {
        if (err) *err = domain_error_validation("Email must not be null");
        return false;
    }

    /* trim left/right */
    const char *start = raw;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')
        ++start;

    const char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\n' || end[-1] == '\r')) {
        --end;
                           }

    size_t len = (size_t)(end - start);
    if (len == 0) {
        if (err) *err = domain_error_validation("Email must not be empty");
        return false;
    }
    if (len > EMAIL_MAX_LEN) {
        if (err) *err = domain_error_validation("Email too long");
        return false;
    }

    char buffer[EMAIL_MAX_LEN + 1];
    memcpy(buffer, start, len);
    buffer[len] = '\0';

    char *at = strchr(buffer, '@');
    if (!at || strchr(at + 1, '@')) {
        if (err) *err =
                domain_error_validation("Email must contain exactly one '@'");
        return false;
    }

    char *local = buffer;
    char *domain = at + 1;
    *at = '\0';

    if (*local == '\0' || *domain == '\0') {
        if (err) *err = domain_error_validation(
                "Email local/domain must not be empty");
        return false;
    }

    if (!strchr(domain, '.')) {
        if (err) *err =
                domain_error_validation("Email domain must contain a '.'");
        return false;
    }

    *at = '@';

    strcpy(out->value, buffer);
    if (err) *err = domain_error_ok();
    return true;
}
