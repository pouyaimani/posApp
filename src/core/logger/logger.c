#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "hal/dev/dev.h"

#if USE_LOG

#

#define LOG_BUFFER_SIZE 256

static LogConfig_t g_cfg;

static const char *level_str[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void initLogger(const LogConfig_t *cfg) {
    g_cfg = *cfg;
}

static size_t format_log_line(char *buf,
                              size_t buf_size,
                              const char *file,
                              int line,
                              const char *fmt,
                              va_list ap)
{
    size_t n = 0;
    DateTime *dt = NULL;

    if (g_cfg.getDateTime) {
        dt = g_cfg.getDateTime(getDevice());
    }

    if (dt) {
        /* date = YYMMDD, time = HHMMSS */
        n += snprintf(buf + n, buf_size - n,
                      "[%c%c-%c%c-%c%c %c%c:%c%c:%c%c]",
                      dt->date[0], dt->date[1],
                      dt->date[2], dt->date[3],
                      dt->date[4], dt->date[5],
                      dt->time[0], dt->time[1],
                      dt->time[2], dt->time[3],
                      dt->time[4], dt->time[5]);
    } else {
        n += snprintf(buf + n, buf_size - n, "[no-time]");
    }

    n += snprintf(buf + n, buf_size - n,
                  "[%s][%s:%d] ",
                  level_str[LOG_LEVEL],
                  file,
                  line);

    n += vsnprintf(buf + n, buf_size - n, fmt, ap);
    n += snprintf(buf + n, buf_size - n, "\n");

    return n;
}

void log_log(const char *file,
             int line,
             const char *fmt, ...)
{
    if (!g_cfg.writer.write) {
        return;
    }

    char buf[LOG_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    size_t len = format_log_line(
        buf, LOG_BUFFER_SIZE, file, line, fmt, ap
    );
    va_end(ap);

    g_cfg.writer.write(getDevice(), buf, len, g_cfg.writer.udata);
}

#endif