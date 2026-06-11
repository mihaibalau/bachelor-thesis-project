# src/ — frontend layout

Entry: `main.tsx` → `AppThemeProvider` → `App.tsx` (router + auth).

```
src/
├── App.tsx              # routes: /login, /register, /app/*
├── theme.ts             # MUI palette + component overrides (dark/light)
├── features/            # one folder per app area — see features/readme.md
└── shared/              # apiClient, hooks, contexts, reusable UI — see shared/readme.md
```

---

## Routing (`App.tsx`)

| Path | Guard | Content |
|------|-------|---------|
| `/login`, `/register` | `PublicOnlyRoute` | Auth forms; redirect to dashboard if already signed in |
| `/app/*` | `ProtectedRoute` | `AppLayout` shell + feature pages |
| `*` | — | Redirect to `/login` |

`AppLayout` wraps pages with `ToastProvider`, `AccountsProvider`, and `AppShell` (sidebar + outlet).

---

## Theme

`features/theme/AppThemeProvider.tsx` reads `gentlix-color-mode` from `localStorage` (default dark), builds the MUI theme via `createAppTheme` in `theme.ts`, and exposes toggle through `ColorModeContext`.

---

## Data flow

1. **Auth** — `features/auth/AuthContext.tsx` persists JWT; `shared/apiClient.ts` attaches it to requests.
2. **Accounts cache** — `shared/context/AccountsContext.tsx` loads `GET /api/users/:id` after login; pages call `reload()` after mutations.
3. **Page data** — most screens use `shared/hooks/useAsyncData.ts` with a fetch function from `features/*/api.ts`.
4. **Cross-page refresh** — `shared/refreshEvents.ts` broadcasts dashboard reload after transactions.

Backend contract: same 22 routes under `/api` for Rust and C — see root project `readme.md`.
