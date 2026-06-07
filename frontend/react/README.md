# Gentlix Bank — React frontend

Vite + React + MUI user app for the Gentlix Bank thesis project.

## Setup

```sh
npm install
cp .env.example .env.local   # optional — defaults to http://localhost:6767/api
npm run dev
```

Open http://localhost:5173. The dev server expects a backend on port **6767** (Rust or C).

## Scripts

| Command | Description |
|---------|-------------|
| `npm run dev` | Start Vite dev server |
| `npm run build` | Typecheck (`tsc -b`) + production build to `dist/` |
| `npm run lint` | ESLint |
| `npm run preview` | Preview production build |

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| `VITE_API_BASE_URL` | `http://localhost:6767/api` | Backend API base URL (read in `src/shared/apiClient.ts`) |

`VITE_API_BASE_URL` is **baked in at build time** (Vite inlines `import.meta.env.*`). For a different API URL in production, set it before `npm run build` (or as a Docker build arg) — it cannot be changed after the bundle is built. See [`../../deploy/AWS.md`](../../deploy/AWS.md) for the nginx-served production image.

## Production build

```sh
VITE_API_BASE_URL=https://api.example.com/api npm run build
# serve the resulting dist/ folder with any static host (nginx, S3 + CloudFront, …)
```

## Auth flow

1. Register or log in via `/login`
2. JWT is stored in memory and sent as `Authorization: Bearer …`
3. On 401 with an active session, the app logs out and redirects to `/login`

## Features

- **Dashboard** — balances, spending chart, recent activity (from `GET /api/dashboard`)
- **Accounts** — list, open, detail
- **Transactions** — record deposits, withdrawals, transfers, sends, payments
- **Statements** — account statement with date/type filters
- **Statistics** — charts from `GET /api/transactions/summary` (all aggregates computed on the backend)
- **Affiliates** — saved payment recipients
