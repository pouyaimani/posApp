#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sys/sys.h"

#if USE_LOG

#define LOG_LOCATION_WIDTH 24
#define LOG_BUFFER_SIZE    256

static const char* level_color[] = {LOG_COLOR_TRACE, LOG_COLOR_DEBUG,
                                    LOG_COLOR_INFO,  LOG_COLOR_WARN,
                                    LOG_COLOR_ERROR, LOG_COLOR_FATAL};

static const char level_char[] = {
    'T', /* TRACE */
    'D', /* DEBUG */
    'I', /* INFO  */
    'W', /* WARN  */
    'E', /* ERROR */
    'F'  /* FATAL */
};

static Logger      __logger;
static LogConfig_t g_cfg;

static void initLogger(const LogConfig_t* cfg) {
    if (cfg != NULL) {
        g_cfg = *cfg;
    }
}

static const char* log_basename(const char* path) {
    if (path == NULL) {
        return "?";
    }

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

/*
 * Append formatted text and return the number of bytes ACTUALLY stored.
 * vsnprintf() returns the number it wanted to store, so never add that
 * return value directly to the current buffer offset.
 *
 * capacity includes room for a terminating NUL. Thus, the returned
 * position never exceeds capacity - 1.
 */
static size_t append_vformat(char* buf, size_t capacity, size_t used,
                             const char* fmt, va_list ap) {
    if (buf == NULL || fmt == NULL || capacity == 0 || used >= capacity)
        return used;

    const size_t available = capacity - used;
    int          required  = vsnprintf(buf + used, available, fmt, ap);

    if (required < 0) {
        buf[used] = '\0';
        return used;
    }

    if ((size_t)required >= available)
        return capacity - 1; /* Truncated, including the terminating NUL. */

    return used + (size_t)required;
}

static size_t append_format(char* buf, size_t capacity, size_t used,
                            const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t new_used = append_vformat(buf, capacity, used, fmt, ap);
    va_end(ap);
    return new_used;
}

static size_t format_log_line(char* buf, size_t buf_size, const char* file,
                              int level, int line, const char* fmt,
                              va_list ap) {
    if (buf == NULL || buf_size < 3 || fmt == NULL || level < LOG_LEV_TRACE ||
        level > LOG_LEV_FATAL) {
        return 0;
    }

    /* Reserve 2 bytes for CRLF, plus 1 byte for NUL. */
    const size_t text_capacity = buf_size - 2;
    size_t       used          = 0;
    buf[0]                     = '\0';

    char location[64];
    (void)snprintf(location, sizeof(location), "%s:%d", log_basename(file),
                   line);

    DateTime* dt = NULL;
    if (g_cfg.getDateTime != NULL)
        dt = g_cfg.getDateTime(sys());

    if (dt != NULL) {
        used = append_format(buf, text_capacity, used, "[%c%c:%c%c:%c%c]  ",
                             dt->time[0], dt->time[1], dt->time[2], dt->time[3],
                             dt->time[4], dt->time[5]);
    } else {
        used = append_format(buf, text_capacity, used, "--:--:--.---  ");
    }

    used = append_format(buf, text_capacity, used, "%s[%c]%s [%-*s]  ",
                         level_color[level], level_char[level], LOG_COLOR_RESET,
                         LOG_LOCATION_WIDTH, location);

    used = append_vformat(buf, text_capacity, used, fmt, ap);

    /* Append CRLF even when the message was truncated. */
    buf[used++] = '\r';
    buf[used++] = '\n';
    buf[used]   = '\0';
    return used;
}

static void logger_log(const char* file, int logLevel, int line,
                       const char* fmt, ...) {
    if (g_cfg.writer.write == NULL || fmt == NULL || logLevel < LOG_LEV_TRACE ||
        logLevel > LOG_LEV_FATAL) {
        return;
    }

    char buf[LOG_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    size_t len =
        format_log_line(buf, sizeof(buf), file, logLevel, line, fmt, ap);
    va_end(ap);

    if (len != 0)
        g_cfg.writer.write(sys(), buf, len, g_cfg.writer.udata);
}

OOP_CTOR(Logger) {
    self->init = initLogger;
    self->log  = logger_log;
}

Logger* logger() {
    CALL_ONCE(OOP_CALL_CTOR(Logger, &__logger););
    return &__logger;
}

#endif
