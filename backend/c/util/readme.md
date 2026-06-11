# util (C)

Small shared helpers with no business knowledge. Used from `main`, repos, server, and services.

- **`dotenv.c`** — load `.env` into the process environment (Rust `dotenvy` equivalent).
- **`log.c`** — `LOG_*` macros, text or JSON lines, level from env.
- **`util_str.c`** — `util_str_to_i64` for parsing PG id columns without accepting `"123abc"`.

| Function | Purpose |
|----------|---------|
| `load_dotenv` / `load_dotenv_first` / `load_dotenv_near_executable` | Find and parse `.env` (Windows: also next to `c.exe`) |
| `log_init` / `log_write` | Tracing-style logging to stdout/stderr |
| `util_str_to_i64` | Strict decimal string → `int64_t` |

Signatures: `util/include/*.h`.
