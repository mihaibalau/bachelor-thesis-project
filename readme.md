gentlix-bank/
├── api/
│   └── openapi.yaml                 # routes + DTO + auth (JWT)
├── db/
│   ├── migrations/                  # SQL migrations (source of truth)
│   └── seed/
├── backend/
│   ├── rust/
│   │   ├── Cargo.toml               # workspace
│   │   ├── crates/
│   │   │   ├── domain/              
│   │   │   ├── service/             # use-cases
│   │   │   ├── db/                  # SQLx + Postgres + migrations runner
│   │   │   ├── api/                 # DTO + OpenAPI (utoipa)
│   │   │   └── server/              # axum REST API (Tokio runtime)
│   └── c/
│       ├── CMakeLists.txt
│       ├── src/
│       │   ├── domain/
│       │   ├── service/
│       │   ├── db/                  # Postgres: libpq [web:95]
│       │   ├── http/                # HTTPS server (ex: libmicrohttpd/civetweb)
│       │   └── auth/                # JWT (ex: libjwt)
├── sdk/
│   ├── typescript/                  # (React + Tauri UI)
│   ├── kotlin/                      # (Android)
│   └── c/                           # wrapper C: functions -> REST calls (for GTK)
├── clients/
│   ├── web-react/                   # User Mode (browser)
│   ├── desktop-tauri-admin/         # Admin Mode (desktop, UI React în WebView)
│   ├── desktop-gtk-admin/           # Admin Mode (C + GTK)
│   └── mobile-android/              # User Mode (Kotlin)
└── infra/
    └── docker-compose.yml           # Postgres + reverse proxy TLS
