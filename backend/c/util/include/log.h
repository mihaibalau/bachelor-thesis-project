#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdarg.h>

typedef enum LogLevel {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_WARN  = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_OFF   = 5
} LogLevel;

// Call once at startup (after dotenv). Reads LOG_LEVEL, DEBUG_MODE, LOG_FORMAT, SERVICE_NAME.
void log_init(void);

bool log_is_debug(void);
LogLevel log_get_level(void);

void log_write(LogLevel level, const char *event, const char *fmt, ...);

#define LOG_TRACE(event, ...) log_write(LOG_LEVEL_TRACE, (event), __VA_ARGS__)
#define LOG_DEBUG(event, ...) log_write(LOG_LEVEL_DEBUG, (event), __VA_ARGS__)
#define LOG_INFO(event, ...)  log_write(LOG_LEVEL_INFO,  (event), __VA_ARGS__)
#define LOG_WARN(event, ...)  log_write(LOG_LEVEL_WARN,  (event), __VA_ARGS__)
#define LOG_ERROR(event, ...) log_write(LOG_LEVEL_ERROR, (event), __VA_ARGS__)

#endif /* LOG_H */
