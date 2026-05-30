#include <stdio.h>
#include <stdlib.h>

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
    fprintf(stdout, "[info] starting Gentlix Bank C backend\n");

    /* 1. DATABASE_URL */
    const char *database_url = getenv("DB_CONN");
    if (!database_url) {
        database_url = "host=localhost port=5432 dbname=gentlix_bank user=mihai password=admingentlix01 sslmode=disable";
    }

    /* 2. DB connect  */
    Db *db = NULL;
    RepoError dberr;
    if (!db_connect(database_url, &db, &dberr)) {
        fprintf(stderr, "[error] failed to connect to DB: %s\n", dberr.message);
        return 1;
    }
    fprintf(stdout, "[info] connected to PostgreSQL\n");

    /* 3. Repositories */
    UserRepo        *user_repo      = user_repo_new(db);
    AccountRepo     *account_repo   = account_repo_new(db);
    TransactionRepo *tx_repo        = transaction_repo_new(db);
    AffiliateRepo   *affiliate_repo = affiliate_repo_new(db);

    if (!user_repo || !account_repo || !tx_repo || !affiliate_repo) {
        fprintf(stderr, "[error] failed to allocate repositories (OOM)\n");
        db_close(db);
        return 1;
    }

    /* 4. Services */

    /* UserService */
    UserRepository    user_port     = user_repository_from_user_repo(user_repo);
    AccountRepository user_acc_port = account_repository_from_account_repo(account_repo);
    UserService *user_svc = user_service_new(user_port, user_acc_port);
    if (!user_svc) {
        fprintf(stderr, "[error] failed to create UserService\n");
        db_close(db);
        return 1;
    }

    /* AccountService */
    AccountServiceRepository acct_port = account_service_repository_from_repo(account_repo);
    AccountService *account_svc = account_service_new(acct_port);
    if (!account_svc) {
        fprintf(stderr, "[error] failed to create AccountService\n");
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    /* TransactionService — tx_repository_from_repo + tx_account_repository_from_repo */
    TransactionRepository tx_port     = tx_repository_from_repo(tx_repo);
    TxAccountRepository   tx_acc_port = tx_account_repository_from_repo(account_repo);
    TransactionService *tx_svc = transaction_service_new(tx_port, tx_acc_port);
    if (!tx_svc) {
        fprintf(stderr, "[error] failed to create TransactionService\n");
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    /* AffiliateService — affiliate + account + user repository ports */
    AffiliateRepository     aff_port      = aff_repository_from_repo(affiliate_repo);
    AffSvcAccountRepository aff_acc_port  = aff_account_repository_from_repo(account_repo);
    AffSvcUserRepository    aff_user_port = aff_user_repository_from_repo(user_repo);
    AffiliateService *affiliate_svc = affiliate_service_new(aff_port, aff_acc_port, aff_user_port);
    if (!affiliate_svc) {
        fprintf(stderr, "[error] failed to create AffiliateService\n");
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    /* 5. JWT_SECRET */
    const char *jwt_secret = getenv("JWT_SECRET");
    if (!jwt_secret) {
        jwt_secret = "a3f8c2d1e4b5a6f7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1";
    }
    // if (!jwt_secret) {
    //     fprintf(stderr, "[error] JWT_SECRET must be set\n");
    //     affiliate_service_free(affiliate_svc);
    //     transaction_service_free(tx_svc);
    //     account_service_free(account_svc);
    //     user_service_free(user_svc);
    //     db_close(db);
    //     return 1;
    // }

    /* 6. AppState */
    AppState state;
    app_state_init(&state, user_svc, account_svc, tx_svc, affiliate_svc, jwt_secret);

    /* 7. HTTP server */
    struct HttpServer srv;
    unsigned short port = 6767;
    if (!http_server_start(&srv, &state, port)) {
        fprintf(stderr, "[error] failed to start HTTP server on port %u\n", port);
        affiliate_service_free(affiliate_svc);
        transaction_service_free(tx_svc);
        account_service_free(account_svc);
        user_service_free(user_svc);
        db_close(db);
        return 1;
    }

    fprintf(stdout, "[info] Listening on http://0.0.0.0:%u\n", port);
    fprintf(stdout, "Press ENTER to stop...\n");
    (void)getchar();

    /* 8. Cleanup */
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