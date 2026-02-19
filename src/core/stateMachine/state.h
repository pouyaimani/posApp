#ifndef STATE_H
#define STATE_H

#include "oop.h"
#include <stdint.h>
#include "core.h"

#define STATE_ENTER(type) type##_Enter

#define STATE_DEF_ENTER(type) static void type##_Enter(State *state)

#define STATE_EXIT(type) type##_Exit

#define STATE_DEF_EXIT(type) static void type##_Exit(State *state)

#define STATE_HANDLE(type, event) \
    type##_handle_##event

#define STATE_DEF_HANDLE(type, event) \
    static void type##_handle_##event(State *state, event## *ev)

#define SM_GOTO(state) getSmCore()->goTo(state)

#define SM_SUB(state) getSmCore()->goToSub(state)

/* Forward */
OOP_DECLARE_CLASS(Event);
OOP_DECLARE_CLASS(TimeOutEvent);
OOP_DECLARE_CLASS(KeypadEvent);
OOP_DECLARE_CLASS(MagEvent);
OOP_DECLARE_CLASS(WifiEvent);
OOP_DECLARE_CLASS(State);

/* ===== Inner state ===== */

typedef enum StateInner {
    STATE_ENTRY = 0,
    STATE_EVENT,
    STATE_EXIT,
    STATE_SUBSTATE
} StateInner;

/* ===== State vtable ===== */

OOP_VTABLE(State)
{
    OOP_IMETHOD(void, State, enter);
    OOP_IMETHOD(void, State, exit);
    OOP_IMETHOD(void, State, handleTimeout, TimeOutEvent *ev);
    OOP_IMETHOD(void, State, handleKeypad, KeypadEvent *ev);
    OOP_IMETHOD(void, State, handleMag, MagEvent *ev);
    OOP_IMETHOD(void, State, handleWifi, WifiEvent *ev);
    // These methods shall not be overrided
    OOP_IMETHOD(void, State, goTo, State *);
    OOP_IMETHOD(void, State, setNext, State *);
    OOP_IMETHOD(void, State, setPrev, State *);
};

/* ===== State base ===== */

OOP_CLASS(State)
{
    OOP_IMPLEMENTS(State);
    StateInner inner;
    const char *name;
    State *parent;
    State *next;
    State *prev;
};

/* ctor */
OOP_CTOR(State, State *parent, const char *name);

typedef State SubState;

#endif
