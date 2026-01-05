#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

typedef enum {
    LOG_LEV_TRACE,
    LOG_LEV_DEBUG,
    LOG_LEV_INFO,
    LOG_LEV_WARN,
    LOG_LEV_ERROR,
    LOG_LEV_FATAL
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
#define LOG_TRACE(...) log_log(LOG_LEV_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_log(LOG_LEV_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_log(LOG_LEV_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_log(LOG_LEV_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_log(LOG_LEV_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_log(LOG_LEV_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
