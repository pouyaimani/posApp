#ifndef KEYPAD_H_
#define KEYPAD_H_

#include "oop.h"
#include <stdbool.h>
#include "core/stateMachine/event.h"

OOP_DECLARE_CLASS(Keypad)

OOP_VTABLE(Keypad) {
    OOP_IMETHOD(void, Keypad, init);
    OOP_IMETHOD(void, Keypad, readKey);
    OOP_IMETHOD(Key_t, Keypad, getKey);
    OOP_IMETHOD(bool, Keypad, isPressed);
};

OOP_CLASS(Keypad) {
    OOP_IMPLEMENTS(Keypad);
    Key_t key;
};

OOP_CTOR(Keypad);

Keypad *getKeypad(void);

#define KEYPAD_INIT() OOP_CALL(getKeypad(), init)

#endif