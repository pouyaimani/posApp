#ifndef LOGGER_H
#define LOGGER_H

#define USE_LOG 0

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>
#include "oop.h"

#if USE_LOG

#define    LOG_LEV_TRACE 0
#define    LOG_LEV_DEBUG 1
#define    LOG_LEV_INFO  2
#define    LOG_LEV_WARN  3
#define    LOG_LEV_ERROR 4
#define    LOG_LEV_FATAL 5

#define LOG_LEVEL LOG_LEV_TRACE

OOP_DECLARE_CLASS(DateTime);
OOP_DECLARE_CLASS(Device);

/* Writer interface (device-specific) */
typedef struct {
    void (*write)(Device *dev, const char *data, size_t len, void *udata);
    void *udata;
} LogWriter_t;

/* Logger configuration */
typedef struct {
    LogWriter_t writer;
    DateTime *(*getDateTime)(Device *dev);
} LogConfig_t;

/* API */
void initLogger(const LogConfig_t *cfg);
void log_log(
             const char *file,
             int line,
             const char *fmt, ...);

/* Convenience macros */
#define LOG_TRACE(...) log_log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_log(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_log(__FILE__, __LINE__, __VA_ARGS__)

/**********************
 *      MACROS
 **********************/
#ifndef LOG_TRACE
#  if LOG_LEVEL <= LOG_LEVEL_TRACE
#    define LOG_TRACE(...) log_log(LOG_LEV_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#  else
#    define LOG_TRACE(...) do {}while(0)
#  endif
#endif

#ifndef LOG_INFO
#  if LOG_LEVEL <= LOG_LEVEL_INFO
#    define LOG_INFO(...) log_log(LOG_LEV_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#  else
#    define LOG_INFO(...) do {}while(0)
#  endif
#endif

#ifndef LOG_WARN
#  if LOG_LEVEL <= LOG_LEVEL_WARN
#    define LOG_WARN(...) log_log(LOG_LEV_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#  else
#    define LOG_WARN(...) do {}while(0)
#  endif
#endif

#ifndef LOG_ERROR
#  if LOG_LEVEL <= LOG_LEVEL_ERROR
#    define LOG_ERROR(...) log_log(LOG_LEV_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#  else
#    define LOG_ERROR(...) do {}while(0)
#  endif
#endif


#ifndef LOG_FATAL
#  if LOG_LEVEL < LOG_LEVEL_NONE
#    define LOG_FATAL(...) log_log(LOG_LEV_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#  else
#    define LOG_FATAL(...) do {} while(0)
#  endif
#endif

#else /*USE_LOG*/

/*Do nothing if `LV_USE_LOG 0`*/
#define LOG_TRACE(...) do {}while(0)
#define LOG_INFO(...) do {}while(0)
#define LOG_WARN(...) do {}while(0)
#define LOG_ERROR(...) do {}while(0)
#define LOG_FATAL(...) do {}while(0)
#endif /*USE_LOG*/

#endif