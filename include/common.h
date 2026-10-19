#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include "logger.h"
#include <stdint.h>
#include "error.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define MERCHANT_DEFAULT_PIN "1111"

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

#define SECS(x) (x * 1000)

#define MINUTES(x) (x * 60 * 1000)

#endif