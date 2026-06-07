# syntax=docker/dockerfile:1

# ---- Build stage ----
# Rust edition 2024 requires a recent toolchain (>= 1.85).
FROM rust:1-bookworm AS build
WORKDIR /app

# sqlx::query! macros are verified at COMPILE TIME. With no live database
# reachable during the build, you MUST commit an offline cache:
#   cd backend/rust && cargo sqlx prepare   # generates .sqlx/
# and build with SQLX_OFFLINE=true (set below). Without .sqlx/ this build fails.
ENV SQLX_OFFLINE=true

COPY . .
RUN cargo build --release --locked

# ---- Runtime stage ----
FROM debian:bookworm-slim AS runtime
WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Non-root runtime user
RUN useradd --system --no-create-home --uid 10001 appuser
USER appuser

COPY --from=build /app/target/release/rust /usr/local/bin/gentlix-rust

# DATABASE_URL, JWT_SECRET, CORS_ORIGIN, PORT, LOG_* are supplied at runtime
# (compose env / ECS task definition). Do NOT bake secrets into the image.
ENV PORT=6767
EXPOSE 6767

CMD ["gentlix-rust"]
