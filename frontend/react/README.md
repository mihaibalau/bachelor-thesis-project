# React Frontend — Architecture Overview

Gentlix Bank user app (Vite + React + MUI). Calls either Rust or C backend on port **6767** — same `/api` routes, same JSON shapes.

**Layer docs:**

| Layer | Folder | Readme |
|-------|--------|--------|
| Routing, pages, API wrappers | `src/features/` | [src/features/readme.md](src/features/readme.md) |
| HTTP client, hooks, shared UI | `src/shared/` | [src/shared/readme.md](src/shared/readme.md) |
| MUI theme tokens | `src/theme.ts` | (inline — see below) |

In-code stack overview: [src/readme.md](src/readme.md)

---

## Layer stack

```
  Browser
       │  fetch + Bearer JWT
       ▼
┌──────────────────────────────────────────┐
│  shared/apiClient.ts                     │  base URL, auth header, ApiError
└──────────────────┬───────────────────────┘
                   │
       ┌───────────┴───────────┐
       ▼                       ▼
┌──────────────┐      ┌────────────────────┐
│ features/*/  │      │ shared/context     │
│ api.ts pages │      │ hooks, components  │
└──────────────┘      └────────────────────┘
```

Pages live under `src/features/<area>/`. Cross-cutting pieces (formatting, toasts, account list cache) sit in `src/shared/`.

---

## Setup

```sh
npm install
cp .env.example .env.local   # optional — defaults to http://localhost:6767/api
npm run dev
```

Open http://localhost:5173. Expect a backend on port **6767** (Rust or C — not both).

| Command | Description |
|---------|-------------|
| `npm run dev` | Vite dev server |
| `npm run build` | `tsc -b` + production bundle in `dist/` |
| `npm run lint` | ESLint |
| `npm run preview` | Serve `dist/` locally |

---

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| `VITE_API_BASE_URL` | `http://localhost:6767/api` | Backend API base (read in `src/shared/apiClient.ts`) |

`VITE_API_BASE_URL` is **baked in at build time**. For production, set it before `npm run build` or pass it as a Docker build arg. See [`../../deploy/AWS.md`](../../deploy/AWS.md).

Static assets: `public/logo.png` (favicon + branding on login and in the shell).

---

## Auth flow

1. Register or log in at `/login` or `/register`.
2. JWT stored in `localStorage` and sent as `Authorization: Bearer …` on every request.
3. On `401` with an active session, `apiClient` clears auth and redirects to `/login`.

Route guards: `ProtectedRoute` (signed-in `/app/*`), `PublicOnlyRoute` in `App.tsx` (redirect away from login when already signed in).

---

## Features (pages)

| Route | Page | Backend endpoints |
|-------|------|-------------------|
| `/app/dashboard` | Dashboard | `GET /api/dashboard` |
| `/app/accounts` | Accounts list + open dialog | `GET /api/accounts/availability`, `POST /api/accounts` |
| `/app/accounts/:id` | Account detail | `GET /api/accounts/:id` |
| `/app/transactions` | Deposits, withdrawals, transfers, … | `POST /api/transactions/*` |
| `/app/statements` | Filtered statement | `GET /api/transactions/statement` |
| `/app/statistics` | Charts from summary API | `GET /api/transactions/summary` |
| `/app/affiliates` | Saved recipients | `GET/POST/PATCH/DELETE /api/affiliates` |

Money-moving operations are handled entirely on the backend inside DB transactions; the UI only submits forms and refreshes cached account data.

---

## Production build

```sh
VITE_API_BASE_URL=https://api.example.com/api npm run build
```

Serve `dist/` with nginx (see `Dockerfile` + `nginx.conf`) or any static host.
