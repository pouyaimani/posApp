#ifndef LOGGER_H
#define LOGGER_H

#define USE_LOG 1

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>
#include "oop.h"

#if USE_LOG

#define LOG_LEV_TRACE 0
#define LOG_LEV_DEBUG 1
#define LOG_LEV_INFO  2
#define LOG_LEV_WARN  3
#define LOG_LEV_ERROR 4
#define LOG_LEV_FATAL 5

#define LOG_COLOR_RESET "\x1b[0m"

#define LOG_COLOR_TRACE "\x1b[90m" // Gray
#define LOG_COLOR_DEBUG "\x1b[36m" // Cyan
#define LOG_COLOR_INFO  "\x1b[32m" // Green
#define LOG_COLOR_WARN  "\x1b[33m" // Yellow
#define LOG_COLOR_ERROR "\x1b[31m" // Red
#define LOG_COLOR_FATAL "\x1b[91m" // Bright Red

#define LOG_COLOR_RED LOG_COLOR_ERROR

#define LOG_LEVEL LOG_LEV_TRACE

OOP_DECLARE_CLASS(DateTime);
OOP_DECLARE_CLASS(System);

/* Writer interface (device-specific) */
typedef struct {
    void (*write)(System* sys, const char* data, size_t len, void* udata);
    void* udata;
} LogWriter_t;

/* Logger configuration */
typedef struct {
    LogWriter_t writer;
    DateTime* (*getDateTime)(System* sys);
} LogConfig_t;

OOP_CLASS(Logger) {
    OOP_METHOD(void, init, const LogConfig_t*);
    OOP_METHOD(void, log, const char* file, int logLevel, int line,
               const char* fmt, ...);
    LogConfig_t cfg;
};

Logger* logger();

/**********************
 *      MACROS
 **********************/
#ifndef LOG_TRACE
#if LOG_LEVEL <= LOG_LEV_TRACE
#define LOG_TRACE(...)                                                         \
    logger()->log(__FILE__, LOG_LEV_TRACE, __LINE__, __VA_ARGS__)
#else
#define LOG_TRACE(...)                                                         \
    do {                                                                       \
    } while (0)
#endif
#endif

#ifndef LOG_DEBUG
#if LOG_LEVEL <= LOG_LEV_DEBUG
#define LOG_DEBUG(...)                                                         \
    logger()->log(__FILE__, LOG_LEV_DEBUG, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...)                                                         \
    do {                                                                       \
    } while (0)
#endif
#endif

#ifndef LOG_INFO
#if LOG_LEVEL <= LOG_LEV_INFO
#define LOG_INFO(...)                                                          \
    logger()->log(__FILE__, LOG_LEV_INFO, __LINE__, __VA_ARGS__)
#else
#define LOG_INFO(...)                                                          \
    do {                                                                       \
    } while (0)
#endif
#endif

#ifndef LOG_WARN
#if LOG_LEVEL <= LOG_LEV_WARN
#define LOG_WARN(...)                                                          \
    logger()->log(__FILE__, LOG_LEV_WARN, __LINE__, __VA_ARGS__)
#else
#define LOG_WARN(...)                                                          \
    do {                                                                       \
    } while (0)
#endif
#endif

#ifndef LOG_ERROR
#if LOG_LEVEL <= LOG_LEV_ERROR
#define LOG_ERROR(...)                                                         \
    logger()->log(__FILE__, LOG_LEV_ERROR, __LINE__, __VA_ARGS__)
#else
#define LOG_ERROR(...)                                                         \
    do {                                                                       \
    } while (0)
#endif
#endif

#ifndef LOG_FATAL
#if LOG_LEVEL < LOG_LEV_FATAL
#define LOG_FATAL(...)                                                         \
    logger()->log(__FILE__, LOG_LEV_FATAL, __LINE__, __VA_ARGS__)
#else
#define LOG_FATAL(...)                                                         \
    do {                                                                       \
    } while (0)
#endif
#endif

#else /*USE_LOG*/

/*Do nothing if `LV_USE_LOG 0`*/
#define LOG_TRACE(...)                                                         \
    do {                                                                       \
    } while (0)
#define LOG_DEBUG(...)                                                         \
    do {                                                                       \
    } while (0)
#define LOG_INFO(...)                                                          \
    do {                                                                       \
    } while (0)
#define LOG_WARN(...)                                                          \
    do {                                                                       \
    } while (0)
#define LOG_ERROR(...)                                                         \
    do {                                                                       \
    } while (0)
#define LOG_FATAL(...)                                                         \
    do {                                                                       \
    } while (0)
#endif /*USE_LOG*/

#define TRACE_POINT                                                            \
    for (int i = 0; i < 10; i++)                                               \
    LOG_DEBUG("========== %s:%d ==========", __func__, __LINE__)

/*=====================================================
 *
 *                  Module loggers
 *
 *=====================================================*/

#define LOGGER_PED_ENABLE     0
#define LOGGER_FILE_ENABLE    0
#define LOGGER_ISO8583_ENABLE 0
#define LOGGER_NFC_ENABLE     0
#define LOGGER_TMS_ENABLE     0
#define LOGGER_TXN_ENABLE     0
#define LOGGER_EMV_ENABLE     0
#define LOGGER_EVL_ENABLE     0
#define LOGGER_WIFI_AC_ENABLE 0
#define LOGGER_EDB_ENABLE     0

#if LOGGER_PED_ENABLE
#define LOG_PED(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_PED(...) ;
#endif

#if LOGGER_FILE_ENABLE
#define LOG_FILE(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_FILE(...) ;
#endif

#if LOGGER_ISO8583_ENABLE
#define LOG_ISO(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_ISO(...) ;
#endif

#if LOGGER_NFC_ENABLE
#define LOG_NFC(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_NFC(...) ;
#endif

#if LOGGER_TMS_ENABLE
#define LOG_TMS(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_TMS(...) ;
#endif

#if LOGGER_TXN_ENABLE
#define LOG_TXN(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_TXN(...) ;
#endif

#if LOGGER_EMV_ENABLE
#define LOG_EMV(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_EMV(...) ;
#endif

#if LOGGER_EVL_ENABLE
#define LOG_EVL(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_EVL(...) ;
#endif

#if LOGGER_WIFI_AC_ENABLE
#define LOG_WAC(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_WAC(...) ;
#endif

#if LOGGER_EDB_ENABLE
#define LOG_EDB(...) LOG_TRACE(__VA_ARGS__)
#else
#define LOG_EDB(...) ;
#endif

#endif