#ifndef C_REPO_ERROR_H
#define C_REPO_ERROR_H

#include "../../domain/include/error.h"
#include <stdbool.h>

#define REPO_ERROR_MESSAGE_MAX 256

typedef enum {
    REPO_ERROR_NONE = 0,
    REPO_ERROR_DB,
    REPO_ERROR_NOT_FOUND,
    REPO_ERROR_DOMAIN
} RepoErrorCode;

typedef struct {
    RepoErrorCode code;
    char message[REPO_ERROR_MESSAGE_MAX];
} RepoError;

RepoError repo_error_ok(void);
RepoError repo_error_db(const char *msg);
RepoError repo_error_not_found(const char *entity);
RepoError repo_error_from_domain(const DomainError *domain);
bool      repo_error_is_ok(const RepoError *err);

#endif //C_REPO_ERROR_H