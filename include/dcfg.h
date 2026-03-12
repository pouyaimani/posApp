#ifndef DCFG_H_
#define DCFG_H_

#include "device_config.h"

/*====================
   Device related macros
 *====================*/
#ifndef DISP_HOR_RES
#error Please define the macro DISP_HOR_RES with the actual screen width.
#endif

#ifndef DISP_VER_RES
#error Please define the macro DISP_HOR_RES with the actual screen height.
#endif

#ifndef COLOR_DEPTH
#error Please define the macro COLOR_DEPTH with the actual screen color depth.
#endif

#define WIFI_AP_LIST_SIZE 50

#define MAIN_THEME_COLOR 0x652D90

#define MAX_SERVICE_NUM 10

#define MAX_AMOUNT_LIST 13

#define AMOUNT_MAX_CNT  12

#endif