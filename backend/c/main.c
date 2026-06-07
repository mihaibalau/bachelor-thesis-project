#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/include/dotenv.h"
#include "util/include/log.h"
#include "db/include/db.h"
#include "db/include/repo_error.h"
#include "db/include/user_repo.h"
#include "db/include/account_repo.h"
#include "db/include/transaction_repo.h"
#include "db/include/affiliate_repo.h"

#include "service/include/user_service.h"
#include "service/include/account_service.h"
#include "service/include/transaction_service.h"
#include "service/include/affiliate_service.h"

#include "server/include/http_state.h"
#include "server/include/http_server.h"

int main(void) {
    // 0. Load .env from cwd, then next to c.exe (CLion cmake-build-debug).
    static const char *const env_paths[] = { ".env", "../.env" };
    bool loaded = load_dotenv_first(env_paths, sizeof env_paths / sizeof env_paths[0]);
    if (!loaded) {
        loaded = load_dotenv_near_executable();
    }
    log_init();

    if (!loaded) {
        LOG_WARN("startup", "no .env file found");
    } else {
        LOG_DEBUG("startup", ".env loaded");
    }

    // 1. Resolve DATABASE_URL (DB_CONN kept for compatibility).
    const char *database_url = getenv("DATABASE_URL");
    if (!database_url) {
        database_url = getenv("DB_CONN");
    }
    if (!database_url) {
        LOG_ERROR("startup", "DATABASE_URL (or DB_CONN) must be set");
        return 1;
    }

    // 2. Connect to PostgreSQL.
    Db *db = NULL;
    RepoError dberr;
    if (!db_connect(database_url, &db, &dberr)) {
        LOG_ERROR("db_connect", "failed to connect to DB: %s", dberr.message);
        return 1;
    }
    LOG_INFO("db_connect", "connected to PostgreSQL");

    // 3. Allocate repositories.
    UserRepo        *user_repo      = user_repo_new(db);
    AccountRepo     *account_repo   = account_repo_new(db);
    TransactionRepo *tx_repo        = transaction_repo_new(db);
    AffiliateRepo   *affiliate_repo = affiliate_repo_new(db);

    if (!user_repo || !account_repo || !tx_repo || !affiliate_repo) {
        LOG_ERROR("startup", "failed to allocate repositories (OOM)");
        db_close(db);
        return 1;
    }

    // 4. Wire services (ports-and-adapters).

    // 4a. UserService
    UserRepository    user_port     = user_repository_from_user_repo(user_repo);
    AccountRepository user_acc_port = account_repository_from_account_repo(account_repo);
    UserService *user_svc = user_service_new(user_port, user_acc_port);
    if (!user_svc) {
        LOG_ERROR("startup", "failed to create UserService");
        db_close(db);
        return 1;
    }

    // 4b. AccountService
    AccountServiceRepository acct_port = account_service_repository_from_repo(account_repo);
    AccountService *account_svc = account_service_new(acct_port);
    if (!account_svc) {
        LOG_ERROR("startup", "failed to create AccountService");
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    // 4c. TransactionService
    TransactionRepository tx_port     = tx_repository_from_repo(tx_repo);
    TxAccountRepository   tx_acc_port = tx_account_repository_from_repo(account_repo);
    TransactionService *tx_svc = transaction_service_new(tx_port, tx_acc_port);
    if (!tx_svc) {
        LOG_ERROR("startup", "failed to create TransactionService");
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    // 4d. AffiliateService
    AffiliateRepository     aff_port      = aff_repository_from_repo(affiliate_repo);
    AffSvcAccountRepository aff_acc_port  = aff_account_repository_from_repo(account_repo);
    AffSvcUserRepository    aff_user_port = aff_user_repository_from_repo(user_repo);
    AffiliateService *affiliate_svc = affiliate_service_new(aff_port, aff_acc_port, aff_user_port);
    if (!affiliate_svc) {
        LOG_ERROR("startup", "failed to create AffiliateService");
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    // 5. Validate JWT_SECRET.
    const char *jwt_secret = getenv("JWT_SECRET");
    if (!jwt_secret || !jwt_secret[0]) {
        LOG_ERROR("startup", "JWT_SECRET must be set - copy .env.example to .env");
        affiliate_service_free(affiliate_svc);
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }
    if (strlen(jwt_secret) >= JWT_SECRET_MAX) {
        LOG_ERROR("startup", "JWT_SECRET exceeds max length (%d bytes)", JWT_SECRET_MAX - 1);
        affiliate_service_free(affiliate_svc);
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    // 6. Build AppState.
    AppState state;
    app_state_init(&state, user_svc, account_svc, tx_svc, affiliate_svc, jwt_secret);

    // 7. Start HTTP server.
    struct HttpServer srv;
    const char *port_env = getenv("PORT");
    unsigned short port = 6767;
    if (port_env && port_env[0]) {
        int parsed = atoi(port_env);
        if (parsed > 0 && parsed <= 65535) {
            port = (unsigned short)parsed;
        } else {
            LOG_WARN("startup", "invalid PORT=%s, using default 6767", port_env);
        }
    }
    if (!http_server_start(&srv, &state, port)) {
        LOG_ERROR("startup", "failed to start HTTP server on port %u", port);
        affiliate_service_free(affiliate_svc);
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    LOG_INFO("listening", "Listening on http://0.0.0.0:%u", port);
    fprintf(stdout, "Press ENTER to stop...\n");
    (void)getchar();

    // 8. Shutdown and free resources.
    http_server_stop(&srv);
    affiliate_service_free(affiliate_svc);
    transaction_service_free(tx_svc);
    account_service_free(account_svc);
    user_service_free(user_svc);

    affiliate_repo_free(affiliate_repo);
    transaction_repo_free(tx_repo);
    account_repo_free(account_repo);
    user_repo_free(user_repo);

    db_close(db);
    return 0;
}