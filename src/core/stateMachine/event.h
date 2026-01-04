#ifndef EVENT_H
#define EVENT_H

#include "oop.h"

typedef enum SmEventType_t {
    SM_EVENT_TIME_OUT,
    SM_EVENT_KEYPAD
} SmEventType_t;

/* Forward declarations */
OOP_DECLARE_CLASS(State);
OOP_DECLARE_CLASS(Core);
OOP_DECLARE_CLASS(Event);
OOP_DECLARE_CLASS(TimeOutEvent);
OOP_DECLARE_CLASS(KeypadEvent);

/* ===== Event vtable ===== */

OOP_VTABLE(Event)
{
    OOP_IMETHOD(void, Event, dispatch, State *state);
    OOP_IMETHOD(void, Event, dispatchTo, State *state);
};

/* ===== Event base ===== */

OOP_CLASS(Event)
{
    OOP_IMPLEMENTS(Event);
    State *target;
};

/* Base API */
OOP_CTOR(Event);

Event **createEvent(SmEventType_t type);

/* ===== TimeOut ===== */

OOP_CLASS(TimeOutEvent)
{
    OOP_EXTENDS(Event);
};

/* ctor */
OOP_CTOR(TimeOutEvent);

/* ===== Keypad ===== */

typedef enum
{
    KEY_1 = 1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_CLEAR,
    KEY_0,
    KEY_ENTER
} KeypadKey;

OOP_CLASS(KeypadEvent)
{
    OOP_EXTENDS(Event);
    KeypadKey key;
    const char *keyStr;
};

/* ctor */
OOP_CTOR(KeypadEvent, KeypadKey key, const char *str);

#endif
