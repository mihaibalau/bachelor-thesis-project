# domain (C)

Entities and value types shared by `db/` and `service/`. Validation happens here (`email_try_create`, `account_debit`, …) and surfaces as `DomainError`.

```
account.c, user.c, transaction.c, affiliate.c
value/  — email, iban, enums (account_type, currency, transaction_type)
error.c — DomainError helpers
```

**Entities** have identity (`UserId`, …): `*_create` for new rows, `*_rehydrate` from DB, `*_free` for heap cleanup.

**Value objects** (`Email`, `IBAN`, …): `*_try_create` / `*_from_str` — invalid input returns `DomainError`, never a half-valid object.

Rust side: `backend/rust/src/domain/` (same concepts).

Stack overview: [../readme.md](../readme.md).
