#include "include/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static LogLevel g_level = LOG_LEVEL_INFO;
static bool g_json = false;
static bool g_debug_mode = false;
static char g_service[64] = "gentlix-c";

static bool env_truthy(const char *value) {
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0
        || strcasecmp(value, "true") == 0
        || strcasecmp(value, "yes") == 0
        || strcasecmp(value, "on") == 0;
}

static LogLevel parse_level(const char *value) {
    if (!value || !value[0]) return LOG_LEVEL_INFO;
    if (strcasecmp(value, "trace") == 0) return LOG_LEVEL_TRACE;
    if (strcasecmp(value, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(value, "info") == 0)  return LOG_LEVEL_INFO;
    if (strcasecmp(value, "warn") == 0 || strcasecmp(value, "warning") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(value, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(value, "off") == 0)   return LOG_LEVEL_OFF;
    return LOG_LEVEL_INFO;
}

static const char *level_name(LogLevel level) {
    switch (level) {
    case LOG_LEVEL_TRACE: return "TRACE";
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO";
    case LOG_LEVEL_WARN:  return "WARN";
    case LOG_LEVEL_ERROR: return "ERROR";
    default:              return "OFF";
    }
}

static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    if (!src) {
        if (cap > 0) dst[0] = '\0';
        return;
    }
    for (size_t i = 0; src[i] != '\0' && j + 2 < cap; ++i) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= cap) break;
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= cap) break;
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= cap) break;
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if ((unsigned char)c < 0x20) {
            continue;
        } else {
            dst[j++] = c;
        }
    }
    if (cap > 0) dst[j < cap ? j : cap - 1] = '\0';
}

void log_init(void) {
    // 1. Read env (SERVICE_NAME, DEBUG_MODE, LOG_LEVEL, LOG_FORMAT).
    const char *service = getenv("SERVICE_NAME");
    if (service && service[0]) {
        snprintf(g_service, sizeof g_service, "%s", service);
    }

    g_debug_mode = env_truthy(getenv("DEBUG_MODE"));

    const char *level_env = getenv("LOG_LEVEL");
    g_level = parse_level(level_env);
    if (g_debug_mode && g_level > LOG_LEVEL_DEBUG) {
        g_level = LOG_LEVEL_DEBUG;
    }

    const char *format = getenv("LOG_FORMAT");
    g_json = format && strcasecmp(format, "json") == 0;

    char ts[32];
    time_t now = time(NULL);
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &utc);

    if (g_json) {
        fprintf(stdout,
            "{\"ts\":\"%s\",\"level\":\"INFO\",\"service\":\"%s\",\"event\":\"tracing_initialized\","
            "\"debug_mode\":%s,\"log_format\":\"json\"}\n",
            ts, g_service, g_debug_mode ? "true" : "false");
    } else {
        fprintf(stdout,
            "[info] service=%s event=tracing_initialized debug_mode=%s log_format=text\n",
            g_service, g_debug_mode ? "true" : "false");
    }
    fflush(stdout);
}

bool log_is_debug(void) {
    return g_debug_mode || g_level <= LOG_LEVEL_DEBUG;
}

LogLevel log_get_level(void) {
    return g_level;
}

void log_write(LogLevel level, const char *event, const char *fmt, ...) {
    if (!event || !event[0]) event = "log";
    // 1. Filter by configured level.
    if (level < g_level) return;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);

    FILE *out = (level >= LOG_LEVEL_WARN) ? stderr : stdout;

    if (g_json) {
        char ts[32];
        time_t now = time(NULL);
        struct tm utc;
#ifdef _WIN32
        gmtime_s(&utc, &now);
#else
        gmtime_r(&now, &utc);
#endif
        strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &utc);

        char esc_event[128];
        char esc_msg[1200];
        json_escape(event, esc_event, sizeof esc_event);
        json_escape(message, esc_msg, sizeof esc_msg);

        fprintf(out,
            "{\"ts\":\"%s\",\"level\":\"%s\",\"service\":\"%s\",\"event\":\"%s\",\"message\":\"%s\"}\n",
            ts, level_name(level), g_service, esc_event, esc_msg);
    } else {
        fprintf(out, "[%s] service=%s event=%s %s\n",
            level_name(level), g_service, event, message);
    }
    fflush(out);
}
