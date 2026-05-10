#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>

typedef void (*CallBack_t)(void *arg);

#define MERCHANT_PIN_LEN                                4
#define MERCHANT_DEFAULT_PIN                            "1111"
#define SYS_PWD_LEN                                     8

#define MAX_REF_NUM_IN_LEN                              12
#define MAX_TRACE_IN_LEN                                6

#define MAX_DATE_IN_LEN                                 8
#define MAX_TIME_IN_LEN                                 6

#define WIFI_AP_LIST_SIZE                               50

#define MAIN_THEME_COLOR                                0x652D90

#define MAX_SERVICE_NUM                                 10

#define MAX_AMOUNT_LIST                                 13

#define AMOUNT_MAX_CNT                                  12

typedef enum {
    EN = 0,
    FA
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

#endif