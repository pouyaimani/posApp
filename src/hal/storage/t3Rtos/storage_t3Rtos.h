#ifndef STORAGE_T3RTOS_H_
#define STORAGE_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../storage.h"

OOP_CLASS(StorageT3Rtos) {
    OOP_EXTENDS(Storage);
};

OOP_CTOR(StorageT3Rtos);

#endif

#endif