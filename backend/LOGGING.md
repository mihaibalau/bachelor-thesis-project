# Backend logging (Rust + C)

Both backends write logs to **stdout/stderr** (container-friendly for AWS ECS / CloudWatch).

## Environment variables

| Variable | Rust | C | Default | Purpose |
|----------|------|---|---------|---------|
| `LOG_FORMAT` | yes | yes | `text` | `json` for CloudWatch Logs Insights |
| `LOG_LEVEL` | via `RUST_LOG` | yes | `info` | C: `trace` / `debug` / `info` / `warn` / `error` |
| `RUST_LOG` | yes | — | `info` | Rust filter, e.g. `info,rust=debug,tower_http=warn` |
| `DEBUG_MODE` | yes | yes | off | `1` or `true` enables verbose debug logging |
| `SERVICE_NAME` | yes | yes | `gentlix-rust` / `gentlix-c` | Appears in every log line |
| `PORT` | yes | yes | `6767` | HTTP listen port (ECS task definition) |

## Local development

```env
# backend/rust/.env or backend/c/.env
LOG_FORMAT=text
DEBUG_MODE=1
RUST_LOG=info,rust=debug,tower_http=warn   # Rust only
LOG_LEVEL=debug                             # C only
```

## AWS production (ECS / Fargate)

```env
LOG_FORMAT=json
SERVICE_NAME=gentlix-rust
RUST_LOG=info,tower_http=warn
PORT=6767
```

Task definition: map container port → ALB target group. CloudWatch Logs driver captures stdout/stderr automatically.

## What gets logged

### Every HTTP request
- **method**, **path**, **query**, **request_id** (from `X-Request-Id` or `X-Amzn-Trace-Id`, else auto-generated)
- **status**, **latency_ms** on completion

### Errors (both backends)
- **5xx** → `ERROR` with `code` + `message`
- **4xx** → `WARN` (validation, not found, etc.)
- **DB/repo failures** → `ERROR` with repo message (C: at conversion; Rust: via `api server error`)

### Auth
- Invalid JWT → `DEBUG` (no token content logged)

### Debug mode (`DEBUG_MODE=1`)
- Rust: `rust=debug`, `tower_http=debug`
- C: minimum level `debug` (404 paths, extra API detail)

## CloudWatch Logs Insights examples

Filter 500 errors (JSON format):

```
fields @timestamp, service, event, message, status
| filter event = "api_error" and status >= 500
| sort @timestamp desc
```

Slow requests (> 1000 ms):

```
fields @timestamp, path, latency_ms, status
| filter latency_ms > 1000
| sort latency_ms desc
```

## Parity note

Rust uses **tracing** spans; C uses structured `event=` fields. Both produce one request log + one completion/error path per call, without duplicate per-handler logs.
