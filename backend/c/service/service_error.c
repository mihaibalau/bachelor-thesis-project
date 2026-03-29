#include "include/service_error.h"

#include <string.h>
#include <stdio.h>

static void service_error_set_msg(ServiceError *err, const char *msg) {
    if (!msg) {
        err->message[0] = '\0';
        return;
    }
    strncpy(err->message, msg, SERVICE_ERROR_MESSAGE_MAX - 1);
    err->message[SERVICE_ERROR_MESSAGE_MAX - 1] = '\0';
}

ServiceError service_error_ok(void) {
    ServiceError err;
    err.code = SERVICE_ERROR_NONE;
    err.message[0] = '\0';
    return err;
}

ServiceError service_error_not_found(const char *entity) {
    ServiceError err;
    err.code = SERVICE_ERROR_NOT_FOUND;

    char buf[SERVICE_ERROR_MESSAGE_MAX];
    if (!entity) entity = "entity";
    snprintf(buf, sizeof buf, "%s not found", entity);
    service_error_set_msg(&err, buf);

    return err;
}

ServiceError service_error_conflict(const char *entity, const char *reason) {
    ServiceError err;
    err.code = SERVICE_ERROR_CONFLICT;

    if (!entity && !reason) {
        err.message[0] = '\0';
        return err;
    }

    char buf[SERVICE_ERROR_MESSAGE_MAX];
    if (reason) {
        snprintf(buf, sizeof buf, "%s: %s", entity ? entity : "conflict", reason);
    } else {
        snprintf(buf, sizeof buf, "%s conflict", entity ? entity : "conflict");
    }
    service_error_set_msg(&err, buf);

    return err;
}

ServiceError service_error_validation(const char *msg) {
    ServiceError err;
    err.code = SERVICE_ERROR_VALIDATION;
    service_error_set_msg(&err, msg);
    return err;
}

ServiceError service_error_from_domain(const DomainError *derr) {
    ServiceError err;
    if (!derr || derr->code == DOMAIN_ERROR_NONE) {
        return service_error_ok();
    }

    err.code = SERVICE_ERROR_DOMAIN;
    service_error_set_msg(&err, derr->message);
    return err;
}

ServiceError service_error_from_repo(const RepoError *rerr) {
    ServiceError err;
    if (!rerr || rerr->code == REPO_ERROR_NONE) {
        return service_error_ok();
    }

    err.message[0] = '\0';

    switch (rerr->code) {
    case REPO_ERROR_NOT_FOUND:
        err.code = SERVICE_ERROR_NOT_FOUND;
        service_error_set_msg(&err, rerr->message);
        break;
    case REPO_ERROR_DOMAIN:
        err.code = SERVICE_ERROR_DOMAIN;
        service_error_set_msg(&err, rerr->message);
        break;
    case REPO_ERROR_DB:
    default:
        err.code = SERVICE_ERROR_REPO;
        service_error_set_msg(&err, rerr->message);
        break;
    }
    return err;
}

bool service_error_is_ok(const ServiceError *err) {
    return err == NULL || err->code == SERVICE_ERROR_NONE;
}