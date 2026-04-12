#ifndef FILE_T3RTOS_H_
#define FILE_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../file.h"

OOP_CLASS(FileT3Rtos) {
    OOP_EXTENDS(File);
};

OOP_CTOR(FileT3Rtos);

#endif

#endif