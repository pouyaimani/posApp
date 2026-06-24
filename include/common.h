#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include "logger.h"
#include <stdint.h>
#include "helpers/error.h"

/**********************************************************/
//                         VERSIONS
/**********************************************************/

#define PNA_APP_VERSION                                                        \
    "09012606100R" // brand[09] model[02] date[2410] version[100] //105 -> 106

#define PNA_TMS_VERSION  "1.3.1" // Stable
#define PNA_RELEASE_DATE "1405-02-02"

/**********************************************************/
//                         LANGUAGES
/**********************************************************/
typedef enum { LNG_EN = 0, LNG_FA, LNG_COUNT } Language_t;

typedef void (*CallBack_t)(void* arg);
typedef int8_t (*ErrCallBack_t)(void* arg);

#define MERCHANT_PIN_LEN     4
#define MERCHANT_DEFAULT_PIN "1111"
#define SYS_PWD_LEN          8

#define MAX_REF_NUM_IN_LEN 12
#define MAX_TRACE_IN_LEN   6

#define MAX_DATE_IN_LEN 8
#define MAX_TIME_IN_LEN 6

#define WIFI_AP_LIST_SIZE 50

#define MAX_SERVICE_NUM 10

#define MAX_AMOUNT_LIST 13

#define AMOUNT_MAX_CNT 12
#define AMOUNT_MIN_CNT 4

#define PHRASES_JSON_ADDR   ""
#define BANK_NAME_JSON_ADDR ""

/**********************************************************/
//                         TMS
/**********************************************************/

#define TMS_VERSION_CACHE "/userdata/tms/version"

/**********************************************************/
//                         HELPERS
/**********************************************************/

#define STR_MAX(field) (sizeof(field) - 1)

#define DEFINE_STRING(name, size)                                              \
    char name[size];                                                           \
    memset(name, 0, size);

#define DEFINE_BYTE_ARRAY(name, size)                                          \
    uint8_t name[size];                                                        \
    memset(name, 0, sizeof(name));

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define DATE_TIME_STR(x) DEFINE_STRING(x, 19 + 1)

#define AMOUNT_STR(x) DEFINE_STRING(x, 12 + 1)

#endif