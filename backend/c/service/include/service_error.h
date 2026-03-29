#ifndef C_SERVICE_ERROR_H
#define C_SERVICE_ERROR_H

#include "domain/include/error.h"
#include "db/include/repo_error.h"

#define SERVICE_ERROR_MESSAGE_MAX 256

typedef enum {
    SERVICE_ERROR_NONE = 0,
    SERVICE_ERROR_NOT_FOUND,
    SERVICE_ERROR_CONFLICT,
    SERVICE_ERROR_VALIDATION,
    SERVICE_ERROR_DOMAIN,
    SERVICE_ERROR_REPO
} ServiceErrorCode;

typedef struct ServiceError {
    ServiceErrorCode code;
    char message[SERVICE_ERROR_MESSAGE_MAX];
} ServiceError;

/* Constructors */

ServiceError service_error_ok(void);

ServiceError service_error_not_found(const char *entity);

ServiceError service_error_conflict(const char *entity, const char *reason);

ServiceError service_error_validation(const char *msg);

ServiceError service_error_from_domain(const DomainError *derr);

ServiceError service_error_from_repo(const RepoError *rerr);

/* Helpers */

bool service_error_is_ok(const ServiceError *err);

#endif