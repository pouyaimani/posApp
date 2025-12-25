#ifndef DEV_T3RTOS_H_
#define DEV_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../dev.h"

OOP_CLASS(T3Rtos) {
    OOP_EXTENDS(Device);
};

OOP_CTOR(T3Rtos, const char* name);

#endif

#endif