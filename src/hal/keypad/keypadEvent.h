#ifndef KEYPAD_EVENT_H
#define KEYPAD_EVENT_H

#include "event.h"

typedef enum Key_t {
    KEY_NONE = -1,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_ENTER,
    KEY_ESC,
    KEY_UP,
    KEY_DOWN,
    KEY_CLEAR,
    KEY_ALPHA,
    KEY_FUNCTION,
    KEY_POWER
} Key_t;

OOP_CLASS(KeypadEvent) {
    OOP_EXTENDS(Event);
    Key_t key;
    char  keyStr;
};

/* ctor */
OOP_CTOR(KeypadEvent);

void keypadEventRegister(void);

#endif