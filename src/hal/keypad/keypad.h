#ifndef KEYPAD_H_
#define KEYPAD_H_

#include "oop.h"
#include <stdbool.h>

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

OOP_DECLARE_CLASS(Keypad)

OOP_VTABLE(Keypad) {
    OOP_IMETHOD(void, Keypad, init);
    OOP_IMETHOD(void, Keypad, read);
};

OOP_CLASS(Keypad) {
    OOP_IMPLEMENTS(Keypad);
    OOP_IMETHOD(Key_t, Keypad, getKey);
    OOP_IMETHOD(bool, Keypad, isPressed);
    Key_t key;
};

OOP_CTOR(Keypad);

Keypad *getKeypad(void);

#endif