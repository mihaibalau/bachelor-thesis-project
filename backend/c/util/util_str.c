#include "include/util_str.h"

#include <errno.h>
#include <stdlib.h>

/*
 * Parse a full decimal string into int64 (HTTP query params, PG text columns).
 * Fails on empty input, trailing junk, or errno from strtoll.
 */
bool util_str_to_i64(const char *s, int64_t *out) {
    if (!s || !out) return false;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    *out = (int64_t)v;
    return true;
}
