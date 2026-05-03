#ifndef JWT_UTILS_H
#define JWT_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include "ids.h"
#include "service_error.h"

bool jwt_decode_user_id(
    const char *secret,
    const char *token,
    UserId *out_user_id,
    ServiceError *err
);

bool jwt_encode_user_id(
    const char *secret,
    int64_t user_id,
    char *out_token,
    size_t out_token_size,
    ServiceError *err
);

#endif