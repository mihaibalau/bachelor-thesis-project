# shared/

Cross-cutting frontend code with no page-specific business UI. Used from every feature module.

---

## HTTP

| File | Role |
|------|------|
| `apiClient.ts` | `fetch` wrapper — base URL, JSON, Bearer token, 401 → logout |
| `apiError.ts` | Parse backend `{ status, code, message }` into `ApiError` |

Token wiring: `AuthContext` calls `setAuthToken` / `setUnauthorizedHandler`; `AccountsContext` re-syncs on login.

---

## Contexts

| Context | Purpose |
|---------|---------|
| `AccountsContext` | Cached user + accounts list; `reload()` after open account / transactions |
| `ToastContext` | Success/error snackbars |

---

## Hooks

| Hook | Purpose |
|------|---------|
| `useAsyncData` | Load + reload + loading/error state for a promise factory |
| `useSurfaceStyles` | Shared card / filter field sx for dark and light surfaces |
| `useButtonStyles` | Primary and outlined button sx presets |

---

## Formatting & display

| File | Purpose |
|------|---------|
| `format.ts` | Money (cents → RON), dates, account labels |
| `transactionDisplay.ts` | Map API transaction rows → activity list items (categories, income flag) |
| `transactionIcons.tsx` | MUI icons per transaction type |
| `buttonStyles.ts` | Shared `AUTH_PRIMARY_BUTTON_SX` for auth pages |
| `refreshEvents.ts` | `requestDashboardRefresh()` after money-moving ops |

---

## Components

Reusable UI: `PageHeader`, `ErrorAlert`, `InlineError`, `AccountPicker`, `AccountSelect`, `TransactionActivityList`, `FormInfoNote`.

Layout: `layout/AppShell.tsx` — sidebar nav, mobile drawer, theme toggle, logout.

Feature pages import from here; feature folders should not duplicate formatting or error parsing.
