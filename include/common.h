#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include "logger.h"
#include <stdint.h>
#include "error.h"

/**********************************************************/
//                         VERSIONS
/**********************************************************/

#define PNA_APP_VERSION                                                        \
    "09012606100R" // brand[09] model[02] date[2410] version[100] //105 -> 106

#define PNA_TMS_VERSION  "1.3.1" // Stable
#define PNA_RELEASE_DATE "1405-02-02"

#define MERCHANT_DEFAULT_PIN "1111"
#define PHRASES_JSON_ADDR    ""
#define BANK_NAME_JSON_ADDR  ""

/**********************************************************/
//                         LANGUAGES
/**********************************************************/
typedef enum { LNG_EN = 0, LNG_FA, LNG_COUNT } Language_t;

typedef void (*CallBack_t)(void* arg);
typedef int8_t (*ErrCallBack_t)(void* arg);

/**********************************************************/
//                         TMS
/**********************************************************/

#define TMS_VERSION_CACHE "/userdata/tms/version"

/**********************************************************/
//                         HELPERS
/**********************************************************/

#define STR_MAX(field) (sizeof(field) - 1)

#define DEFINE_STRING(name, size) char name[size] = {0};
#define RESET_STRING(name)        memset(name, 0, sizeof(name));

#define DEFINE_BYTE_ARRAY(name, size) uint8_t name[size] = {0};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define DATE_TIME_STR(x) DEFINE_STRING(x, 19 + 1)

#define AMOUNT_STR(x) DEFINE_STRING(x, 12 + 1)

#endif