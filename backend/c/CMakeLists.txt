cmake_minimum_required(VERSION 3.20)
project(c C)

set(CMAKE_C_STANDARD 17)

# ── MSYS2 UCRT64 prefix ───────────────────────────────────────────────────────
set(MSYS2_PREFIX "C:/msys64/ucrt64")
list(APPEND CMAKE_PREFIX_PATH "${MSYS2_PREFIX}")

# ── PostgreSQL / libpq ────────────────────────────────────────────────────────
find_program(PG_CONFIG pg_config
        HINTS
        "${MSYS2_PREFIX}/bin"
        "C:/Program Files/PostgreSQL/16/bin"
        "C:/Program Files/PostgreSQL/15/bin"
)
if(NOT PG_CONFIG)
    message(FATAL_ERROR "pg_config not found. Install: pacman -S mingw-w64-ucrt-x86_64-postgresql")
endif()

execute_process(COMMAND ${PG_CONFIG} --includedir
        OUTPUT_VARIABLE PG_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --libdir
        OUTPUT_VARIABLE PG_LIB_DIR    OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "PostgreSQL include dir: ${PG_INCLUDE_DIR}")
message(STATUS "PostgreSQL lib dir:     ${PG_LIB_DIR}")

# ── libmicrohttpd ─────────────────────────────────────────────────────────────
find_library(LIBMICROHTTPD
        NAMES microhttpd libmicrohttpd
        HINTS "${MSYS2_PREFIX}/lib"
        REQUIRED
)

# ── jansson ───────────────────────────────────────────────────────────────────
find_library(LIBJANSSON
        NAMES jansson libjansson
        HINTS "${MSYS2_PREFIX}/lib"
        REQUIRED
)

# ── OpenSSL (FOR jwt_utils.c — HMAC-SHA256 + base64) ──────────────────────
find_library(LIBSSL
        NAMES ssl libssl
        HINTS "${MSYS2_PREFIX}/lib"
        REQUIRED
)
find_library(LIBCRYPTO
        NAMES crypto libcrypto
        HINTS "${MSYS2_PREFIX}/lib"
        REQUIRED
)

# ── Argon2 ────────────────────────────────────────────────────────────────────
find_library(LIBARGON2
        NAMES argon2 libargon2
        HINTS "${MSYS2_PREFIX}/lib"
        REQUIRED
)

message(STATUS "libargon2:     ${LIBARGON2}")
message(STATUS "libmicrohttpd: ${LIBMICROHTTPD}")
message(STATUS "libjansson:    ${LIBJANSSON}")
message(STATUS "libssl:        ${LIBSSL}")
message(STATUS "libcrypto:     ${LIBCRYPTO}")

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(c
        main.c

        # domain
        domain/error.c
        domain/include/error.h
        domain/include/ids.h
        domain/include/account_type.h
        domain/value/account_type.c
        domain/include/currency.h
        domain/value/currency.c
        domain/include/email.h
        domain/value/email.c
        domain/include/iban.h
        domain/value/iban.c
        domain/include/transaction_type.h
        domain/value/transaction_type.c
        domain/include/account.h
        domain/account.c
        domain/include/affiliate.h
        domain/affiliate.c
        domain/include/transaction.h
        domain/transaction.c
        domain/include/user.h
        domain/user.c

        # db
        db/include/repo_error.h
        db/repo_error.c
        db/include/db.h
        db/db.c
        db/account_repo.c
        db/include/account_repo.h
        db/include/user_repo.h
        db/user_repo.c
        db/include/transaction_repo.h
        db/transaction_repo.c
        db/include/affiliate_repo.h
        db/affiliate_repo.c

        # util
        util/include/util_str.h
        util/util_str.c
        util/include/dotenv.h
        util/dotenv.c
        util/include/log.h
        util/log.c

        # service
        service/include/service_error.h
        service/service_error.c
        service/include/user_service.h
        service/user_service.c
        service/include/account_service.h
        service/account_service.c
        service/include/transaction_service.h
        service/transaction_service.c
        service/include/affiliate_service.h
        service/affiliate_service.c

        # server
        server/http_state.c
        server/include/http_state.h
        server/http_error.c
        server/include/http_error.h
        server/http_util.c
        server/include/http_util.h
        server/http_auth.c
        server/include/http_auth.h
        server/http_users.c
        server/include/http_users.h
        server/http_accounts.c
        server/include/http_accounts.h
        server/http_affiliates.c
        server/include/http_affiliates.h
        server/http_transactions.c
        server/include/http_transactions.h
        server/http_dashboard.c
        server/include/http_dashboard.h
        server/http_router.c
        server/include/http_router.h
        server/http_server.c
        server/include/http_server.h
        server/jwt_utils.c
        server/include/jwt_utils.h
)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(c PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/domain/include
        ${CMAKE_SOURCE_DIR}/db/include
        ${CMAKE_SOURCE_DIR}/util/include
        ${CMAKE_SOURCE_DIR}/service/include
        ${CMAKE_SOURCE_DIR}/server/include
        "${MSYS2_PREFIX}/include"
        "${PG_INCLUDE_DIR}"
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_directories(c PRIVATE "${PG_LIB_DIR}")

target_link_libraries(c PRIVATE
        ${LIBMICROHTTPD}
        ${LIBJANSSON}
        ${LIBSSL}
        ${LIBCRYPTO}
        ${LIBARGON2}
        pq
)

# IDE runs (CLion) execute from cmake-build-debug/ — copy .env next to c.exe
if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    add_custom_command(TARGET c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/.env"
            "$<TARGET_FILE_DIR:c>/.env"
        COMMENT "Copying .env to build output directory"
    )
endif()

set_target_properties(c PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)