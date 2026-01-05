#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_Level;

/* Writer interface (device-specific) */
typedef struct {
    void (*write)(const char *data, size_t len, void *udata);
    void *udata;
} log_Writer;

/* Logger configuration */
typedef struct {
    log_Writer writer;
    log_Level  level;
} log_Config;

/* API */
void initLogger(const log_Config *cfg);
void log_log(log_Level level,
             const char *file,
             int line,
             const char *fmt, ...);

/* Convenience macros */
#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
