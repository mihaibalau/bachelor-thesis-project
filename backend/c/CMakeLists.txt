cmake_minimum_required(VERSION 4.0)
project(c C)

set(CMAKE_C_STANDARD 17)

# --- PostgreSQL / libpq ---
find_program(PG_CONFIG pg_config
        HINTS
        "C:/msys64/ucrt64/bin"
        "C:/Program Files/PostgreSQL/16/bin"
        "C:/Program Files/PostgreSQL/15/bin"
)

if(NOT PG_CONFIG)
    message(FATAL_ERROR "pg_config not found. Install: pacman -S mingw-w64-ucrt-x86_64-postgresql")
endif()

execute_process(
        COMMAND ${PG_CONFIG} --includedir
        OUTPUT_VARIABLE PG_INCLUDE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
        COMMAND ${PG_CONFIG} --libdir
        OUTPUT_VARIABLE PG_LIB_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "PostgreSQL include dir: ${PG_INCLUDE_DIR}")
message(STATUS "PostgreSQL lib dir:     ${PG_LIB_DIR}")

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/domain/include
        ${CMAKE_SOURCE_DIR}/db/include
        ${CMAKE_SOURCE_DIR}/util/include
)

add_executable(c
        main.c
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
        db/include/repo_error.h
        db/repo_error.c
        db/include/db.h
        db/db.c
        util/include/util_str.h
        util/util_str.c
        db/account_repo.c
        db/include/account_repo.h
        db/include/user_repo.h
        db/user_repo.c
        db/include/transaction_repo.h
        db/transaction_repo.c
        db/include/affiliate_repo.h
        db/affiliate_repo.c
)

target_include_directories(c PRIVATE "${PG_INCLUDE_DIR}")
target_link_directories(c PRIVATE "${PG_LIB_DIR}")
target_link_libraries(c PRIVATE pq)
