#ifndef ERROR_H_
#define ERROR_H_

#include "logger.h"

/**********************************************************/
//                         ERRORS
/**********************************************************/

typedef int8_t Error_t;

#define ERR_OK                0
#define ERR_NOK               -1
#define ERR_NOT_SUPPORTED     -2
#define ERR_BAD_PARAMETER     -3
#define ERR_NULL_PARAMETER    -4
#define ERR_MEMORY_ALLOCATION -5

#define RETURN_VALUE_IF_NOT(expr, expected, on_error, retval)                  \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val != __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_IF_NOT(expr, expected, on_error)                                \
    do {                                                                       \
        __typeof__(expr)     __val = (expr);                                   \
        __typeof__(expected) __exp = (expected);                               \
                                                                               \
        if (__val != __exp) {                                                  \
            LOG_ERROR("Error: %s returns %d (expected %d)", #expr, (int)__val, \
                      (int)__exp);                                             \
                                                                               \
            on_error;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#define RETURN_VALUE_IF_NULL(ptr, on_error, retval)                            \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            LOG_ERROR("Error: %s is NULL", #ptr);                              \
            on_error;                                                          \
            return (retval);                                                   \
        }                                                                      \
    } while (0)

#define RETURN_IF_NULL(ptr, on_error)                                          \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            LOG_ERROR("Error: %s is NULL", #ptr);                              \
            on_error;                                                          \
            return;                                                            \
        }                                                                      \
    } while (0)

#endif