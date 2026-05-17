#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>


/**********************************************************/
//                         ERRORS
/**********************************************************/

#define ERR_OK                       0
#define ERR_NOK                     -1
#define ERR_NOT_SUPPORTED           -2
#define ERR_BAD_PARAMETER           -3
#define ERR_MEMORY_ALLOCATION       -4


typedef void (*CallBack_t)(void *arg);

#define MERCHANT_PIN_LEN                                4
#define MERCHANT_DEFAULT_PIN                            "1111"
#define SYS_PWD_LEN                                     8

#define MAX_REF_NUM_IN_LEN                              12
#define MAX_TRACE_IN_LEN                                6

#define MAX_DATE_IN_LEN                                 8
#define MAX_TIME_IN_LEN                                 6

#define WIFI_AP_LIST_SIZE                               50

#define MAX_SERVICE_NUM                                 10

#define MAX_AMOUNT_LIST                                 13

#define AMOUNT_MAX_CNT                                  12

typedef enum {
    LNG_EN = 0,
    LNG_FA,
    LNG_COUNT
} Language_t;

#define PHRASES_JSON_ADDR   ""
#define BANK_NAME_JSON_ADDR ""

/**********************************************************/
//                         TMS
/**********************************************************/

#define TMS_VERSION_CACHE                               "/userdata/tms/version"


/**********************************************************/
//                         HELPERS
/**********************************************************/

#define STR_MAX(field) (sizeof(field) - 1)

#define DEFINE_STRING(name, size)    \
    char name[size];                 \
    memset(name, 0, sizeof(name));

#define RETURN_VALUE_IF_NOT(expr, expected, on_error, retval)      \
    do {                                                           \
        __typeof__(expr) __val = (expr);                           \
        __typeof__(expected) __exp = (expected);                   \
                                                                   \
        if (__val != __exp) {                                      \
            LOG_ERROR("Error: %s returns %d (expected %d)",          \
                    #expr,                                         \
                    (int)__val,                                    \
                    (int)__exp);                                   \
                                                                   \
            on_error;                                              \
            return (retval);                                       \
        }                                                          \
    } while (0)

#define RETURN_IF_NOT(expr, expected, on_error)                    \
    do {                                                           \
        __typeof__(expr) __val = (expr);                           \
        __typeof__(expected) __exp = (expected);                   \
                                                                   \
        if (__val != __exp) {                                      \
            LOG_ERROR("Error: %s returns %d (expected %d)",        \
                    #expr,                                         \
                    (int)__val,                                    \
                    (int)__exp);                                   \
                                                                   \
            on_error;                                              \
            return;                                                \
        }                                                          \
    } while (0)


#define RETURN_VALUE_IF_NULL(ptr, on_error, retval)                \
    do {                                                           \
        if ((ptr) == NULL) {                                       \
            LOG_ERROR("Error: %s is NULL", #ptr);                    \
            on_error;                                              \
            return (retval);                                       \
        }                                                          \
    } while (0)

#define RETURN_IF_NULL(ptr, on_error)                              \
    do {                                                           \
        if ((ptr) == NULL) {                                       \
            LOG_ERROR("Error: %s is NULL", #ptr);                    \
            on_error;                                              \
            return;                                                \
        }                                                          \
    } while (0)

#define ARRAY_SIZE(x) \
    (sizeof(x) / sizeof((x)[0]))


#define DATE_TIME_STR(x)        DEFINE_STRING(x, 19 + 1)

#define AMOUNT_STR(x)        DEFINE_STRING(x, 12 + 1)


#endif