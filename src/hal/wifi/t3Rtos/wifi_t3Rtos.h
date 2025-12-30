#ifndef WIFI_T3RTOS_H_
#define WIFI_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../wifi.h"

OOP_CLASS(WifiT3Rtos) {
    OOP_EXTENDS(Wifi);
};

OOP_CTOR(WifiT3Rtos);

#endif

#endif