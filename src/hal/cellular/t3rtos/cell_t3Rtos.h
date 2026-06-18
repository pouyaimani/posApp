#ifndef CELL_T3RTOS_H_
#define CELL_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../cellular.h"

OOP_CLASS(CellT3Rtos) { OOP_EXTENDS(Cellular); };

OOP_CTOR(CellT3Rtos);

#endif

#endif