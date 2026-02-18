#ifndef NETWORK_T3RTOS_H_
#define NETWORK_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../network.h"

OOP_CLASS(NetworkT3Rtos) {
    OOP_EXTENDS(Network);
};

OOP_CTOR(NetworkT3Rtos);

#endif

#endif