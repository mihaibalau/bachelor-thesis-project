```
gentlix-bank/
├── api/
│   └── openapi.yaml                 # Contract API: routes + DTO + auth (JWT)
├── db/
│   ├── migrations/                  # SQL migrations (source of truth)
│   └── seed/                        # Seed data (optional)
├── backend/
│   ├── rust/
│   │   ├── Cargo.toml               # Rust workspace (multiple crates) [web:36]
│   │   ├── crates/
│   │   │   ├── domain/              # Business rules & entities (no HTTP, no DB)
│   │   │   ├── service/             # Use-cases (application logic)
│   │   │   ├── db/                  # SQLx + Postgres + migrations runner [web:12]
│   │   │   ├── api/                 # DTO + OpenAPI generation (utoipa)
│   │   │   └── server/              # axum REST API (Tokio runtime) [web:8]
│   │   └── README.md
│   └── c/
│       ├── CMakeLists.txt
│       ├── src/
│       │   ├── domain/              # Business rules & entities (C)
│       │   ├── service/             # Use-cases (C)
│       │   ├── db/                  # Postgres access: libpq
│       │   ├── http/                # HTTPS server (ex: libmicrohttpd/civetweb)
│       │   └── auth/                # JWT (ex: libjwt)
│       └── README.md
├── sdk/
│   ├── typescript/                  # Generated TS client from OpenAPI (React + Tauri UI) [web:79]
│   ├── kotlin/                      # Generated Kotlin client from OpenAPI (Android) [web:79]
│   └── c/                           # C wrapper: functions -> REST calls (used by GTK admin app)
├── clients/
│   ├── web-react/                   # User Mode (browser)
│   ├── desktop-tauri-admin/         # Admin Mode (desktop, React UI in WebView, Rust shell)
│   ├── desktop-gtk-admin/           # Admin Mode (C + GTK)
│   └── mobile-android/              # User Mode (Kotlin)
└── infra/
    └── docker-compose.yml           # Postgres (+ optional reverse proxy TLS)
```