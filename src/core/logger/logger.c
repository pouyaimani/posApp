#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sys/sys.h"

#if USE_LOG

#define LOG_LOCATION_WIDTH 24

#define LOG_COLOR_RESET "\x1b[0m"

#define LOG_COLOR_TRACE "\x1b[90m" // Gray
#define LOG_COLOR_DEBUG "\x1b[36m" // Cyan
#define LOG_COLOR_INFO  "\x1b[32m" // Green
#define LOG_COLOR_WARN  "\x1b[33m" // Yellow
#define LOG_COLOR_ERROR "\x1b[31m" // Red
#define LOG_COLOR_FATAL "\x1b[91m" // Bright Red

static const char* level_color[] = {LOG_COLOR_TRACE, LOG_COLOR_DEBUG,
                                    LOG_COLOR_INFO,  LOG_COLOR_WARN,
                                    LOG_COLOR_ERROR, LOG_COLOR_FATAL};

#define LOG_BUFFER_SIZE 256

static Logger __logger;

static LogConfig_t g_cfg;

static const char level_char[] = {
    'T', // TRACE
    'D', // DEBUG
    'I', // INFO
    'W', // WARN
    'E', // ERROR
    'F'  // FATAL
};
static void initLogger(const LogConfig_t* cfg) { g_cfg = *cfg; }

static const char* log_basename(const char* path) {
    const char* slash1 = strrchr(path, '/');
    const char* slash2 = strrchr(path, '\\');

    if (slash1 && slash2)
        return (slash1 > slash2) ? slash1 + 1 : slash2 + 1;

    if (slash1)
        return slash1 + 1;

    if (slash2)
        return slash2 + 1;

    return path;
}

static size_t format_log_line(char* buf, size_t buf_size, const char* file,
                              int level, int line, const char* fmt,
                              va_list ap) {
    size_t n = 0;

    const char* name = log_basename(file);

    char location[64];
    snprintf(location, sizeof(location), "%s:%d", name, line);

    DateTime* dt = NULL;

    if (g_cfg.getDateTime)
        dt = g_cfg.getDateTime(sys());

    if (dt) {
        n += snprintf(buf + n, buf_size - n, "[%c%c:%c%c:%c%c]  ", dt->time[0],
                      dt->time[1], dt->time[2], dt->time[3], dt->time[4],
                      dt->time[5]); // optional
    } else {
        n += snprintf(buf + n, buf_size - n, "--:--:--.---  ");
    }

    n += snprintf(buf + n, buf_size - n, "%s[%c]%s [%-*s]  ",
                  level_color[level], level_char[level], LOG_COLOR_RESET,
                  LOG_LOCATION_WIDTH, location);

    n += vsnprintf(buf + n, buf_size - n, fmt, ap);

    n += snprintf(buf + n, buf_size - n, "\r\n");

    return n;
}

static void log(const char* file, int logLevel, int line, const char* fmt,
                ...) {
    if (!g_cfg.writer.write) {
        return;
    }

    char buf[LOG_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    size_t len =
        format_log_line(buf, LOG_BUFFER_SIZE, file, logLevel, line, fmt, ap);
    va_end(ap);

    g_cfg.writer.write(sys(), buf, len, g_cfg.writer.udata);
}

OOP_CTOR(Logger) {
    self->init = initLogger;
    self->log  = log;
}

Logger* logger() {
    CALL_ONCE(OOP_CALL_CTOR(Logger, &__logger););
    return &__logger;
}

#endif