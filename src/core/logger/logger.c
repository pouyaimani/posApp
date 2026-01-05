#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_BUFFER_SIZE 256

static log_Config g_cfg;

static const char *level_str[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void initLogger(const log_Config *cfg) {
    g_cfg = *cfg;
}

static size_t format_log_line(char *buf,
                              size_t buf_size,
                              log_Level level,
                              const char *file,
                              int line,
                              const char *fmt,
                              va_list ap)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    size_t n = strftime(
        buf, buf_size,
        "[%Y-%m-%d %H:%M:%S]", &tm
    );

    n += snprintf(
        buf + n, buf_size - n,
        "[%s][%s:%d] ",
        level_str[level],
        file,
        line
    );

    n += vsnprintf(buf + n, buf_size - n, fmt, ap);
    n += snprintf(buf + n, buf_size - n, "\n");

    return n;
}

void log_log(log_Level level,
             const char *file,
             int line,
             const char *fmt, ...)
{
    if (level < g_cfg.level || !g_cfg.writer.write) {
        return;
    }

    char buf[LOG_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    size_t len = format_log_line(
        buf, sizeof(buf), level, file, line, fmt, ap
    );
    va_end(ap);

    g_cfg.writer.write(buf, len, g_cfg.writer.udata);
}
