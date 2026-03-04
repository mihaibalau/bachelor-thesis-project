cmake_minimum_required(VERSION 4.0)
project(c C)

set(CMAKE_C_STANDARD 17)

add_executable(c main.c
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
)
