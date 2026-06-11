# features/

One folder per application area. Each module typically contains:

```
features/<area>/
├── api.ts       # thin wrappers around shared/apiClient
├── types.ts     # response/request TypeScript types
├── *Page.tsx    # route component(s)
└── components/  # optional local UI (e.g. dashboard skeleton)
```

Pages talk to the backend only through `api.ts`. Types mirror backend JSON field names (`snake_case`).

---

## Modules

| Folder | Routes | Main API |
|--------|--------|----------|
| `auth/` | `/login`, `/register` | `POST /api/users/register`, `POST /api/users/login` |
| `app/` | — | `AppLayout` — providers + shell, no API |
| `dashboard/` | `/app/dashboard` | `GET /api/dashboard` |
| `accounts/` | `/app/accounts`, `/app/accounts/:id` | availability, open, detail |
| `transactions/` | `/app/transactions` | deposit, withdrawal, send, transfer, payment, recent |
| `statements/` | `/app/statements` | `GET /api/transactions/statement` |
| `statistics/` | `/app/statistics` | `GET /api/transactions/summary` (high `per_account_limit` for benchmark data) |
| `affiliates/` | `/app/affiliates` | CRUD + resolve-target |
| `users/` | — | `GET /api/users/:id` (used by `AccountsContext`) |
| `theme/` | — | MUI dark/light mode context |

---

## Conventions

- **Forms** — `react-hook-form` + `zod` on auth pages; controlled MUI fields elsewhere.
- **Errors** — catch `ApiError`, show `ErrorAlert` or toast; don't parse raw `fetch` responses in pages.
- **After mutations** — call `AccountsContext.reload()` and/or `requestDashboardRefresh()` so dashboard and balances stay in sync.
- **Filters** — keep draft state separate from `applied` state so charts/statements refetch only on "Apply", not every keystroke.

Stack overview: [../readme.md](../readme.md). Setup and env vars: [../../README.md](../../README.md).
