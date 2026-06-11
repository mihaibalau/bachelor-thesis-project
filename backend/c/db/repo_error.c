#include "include/repo_error.h"

#include <stdio.h>
#include <string.h>

/*
 * RepoError = failures at the DB/repository boundary.
 * Services translate these into ServiceError before returning to HTTP handlers.
 */

RepoError repo_error_ok(void) {
    RepoError e;
    e.code = REPO_ERROR_NONE;
    e.message[0] = '\0';
    return e;
}

RepoError repo_error_db(const char *msg) {
    RepoError e;
    e.code = REPO_ERROR_DB;
    if (msg) {
        strncpy(e.message, msg, REPO_ERROR_MESSAGE_MAX - 1);
        e.message[REPO_ERROR_MESSAGE_MAX - 1] = '\0';
    } else {
        e.message[0] = '\0';
    }
    return e;
}

RepoError repo_error_not_found(const char *entity) {
    RepoError e;
    e.code = REPO_ERROR_NOT_FOUND;
    if (entity) {
        snprintf(e.message, sizeof(e.message), "%s not found", entity);
    } else {
        snprintf(e.message, sizeof(e.message), "entity not found");
    }
    return e;
}

RepoError repo_error_from_domain(const DomainError *domain) {
    RepoError e;
    e.code = REPO_ERROR_DOMAIN;
    if (domain) {
        strncpy(e.message, domain->message, REPO_ERROR_MESSAGE_MAX - 1);
        e.message[REPO_ERROR_MESSAGE_MAX - 1] = '\0';
    } else {
        e.message[0] = '\0';
    }
    return e;
}

bool repo_error_is_ok(const RepoError *err) {
    return err == NULL || err->code == REPO_ERROR_NONE;
}
