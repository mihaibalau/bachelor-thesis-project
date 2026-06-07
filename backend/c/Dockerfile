# syntax=docker/dockerfile:1

# ---- Build stage ----
# The committed CMakeLists.txt has MSYS2/Windows HINTS for find_library/find_program,
# but those are only hints — on Debian, CMake falls back to the standard system paths,
# so the same CMakeLists builds unchanged once the -dev packages below are installed.
FROM debian:bookworm AS build
WORKDIR /src

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        libpq-dev \
        libmicrohttpd-dev \
        libjansson-dev \
        libssl-dev \
        libargon2-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target c -j"$(nproc)"

# ---- Runtime stage ----
FROM debian:bookworm-slim AS runtime
WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libpq5 \
        libmicrohttpd12 \
        libjansson4 \
        libssl3 \
        libargon2-1 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --system --no-create-home --uid 10001 appuser
USER appuser

COPY --from=build /src/build/c /usr/local/bin/gentlix-c

# DATABASE_URL (or DB_CONN), JWT_SECRET, CORS_ORIGIN, PORT, LOG_* come from the
# runtime environment. No .env is copied in (see .dockerignore) so container env
# vars are authoritative.
ENV PORT=6767
EXPOSE 6767

# NOTE: main.c blocks on getchar() for interactive shutdown. In a headless
# container (no TTY/stdin) getchar() returns EOF immediately and the process
# exits. Run with an attached stdin (compose: stdin_open + tty) or adapt main.c
# to wait on a signal before deploying to ECS/Fargate. See deploy/AWS.md.
CMD ["gentlix-c"]
