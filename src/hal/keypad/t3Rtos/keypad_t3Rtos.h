#ifndef KEYPAD_T3RTOS_H_
#define KEYPAD_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../keypad.h"

OOP_CLASS(KeypadT3Rtos) {
    OOP_EXTENDS(Keypad);
};

OOP_CTOR(KeypadT3Rtos);

#endif

#endif