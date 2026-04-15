#ifndef COMMON_H_
#define COMMON_H_

typedef void (*CallBack_t)(void *arg);

#define MERCHANT_PIN_LEN                                4
#define MERCHANT_DEFAULT_PIN                            "1111"

#define MAX_REF_NUM_IN_LEN                              12
#define MAX_TRACE_IN_LEN                                6

#define MAX_DATE_IN_LEN                                 8
#define MAX_TIME_IN_LEN                                 6

#define WIFI_AP_LIST_SIZE                               50

#define MAIN_THEME_COLOR                                0x652D90

#define MAX_SERVICE_NUM                                 10

#define MAX_AMOUNT_LIST                                 13

#define AMOUNT_MAX_CNT                                  12

/**********************************************************/
//                         TMS
/**********************************************************/

#define TMS_VERSION_CACHE                               "/userdata/tms/version"


/**********************************************************/
//                         HELPERS
/**********************************************************/

#define STR_MAX(field) (sizeof(field) - 1)

#endif