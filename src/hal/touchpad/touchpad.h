#ifndef TOUCHPAD_H_
#define TOUCHPAD_H_

#include "oop.h"
#include <stdbool.h>
#include "core/stateMachine/event.h"

typedef int point_t;

typedef enum { TP_STATE_PRESS, TP_STATE_RELEASE, TP_STATE_HOLD } TpState_t;

OOP_DECLARE_CLASS(Touchpad)

OOP_VTABLE(Touchpad) {
    OOP_IMETHOD(void, Touchpad, init);
    OOP_IMETHOD(bool, Touchpad, read);
};

OOP_CLASS(Touchpad) {
    OOP_IMPLEMENTS(Touchpad);
    point_t   x;
    point_t   y;
    TpState_t state;
};

OOP_CTOR(Touchpad);

Touchpad* touchpad(void);

#define TOUCH_INIT() OOP_CALL(touchpad(), init)

#endif