#ifndef STATE_H
#define STATE_H

#include "oop.h"
#include <stdint.h>
#include "core.h"

#define STATE_ENTER() stateEnter

#define STATE_DEF_ENTER() static void stateEnter()

#define STATE_EXIT() stateExit

#define STATE_DEF_EXIT() static void stateExit()

#define STATE_HANDLE(type) \
    type##_handle

#define STATE_DEF_HANDLE(type) \
    static void type##_handle(State *state, type## *ev)

#define SM_GOTO(state) getSmCore()->goTo(state)

/* Forward */
OOP_DECLARE_CLASS(Event);
OOP_DECLARE_CLASS(TimeOutEvent);
OOP_DECLARE_CLASS(KeypadEvent);
OOP_DECLARE_CLASS(MagEvent);
OOP_DECLARE_CLASS(WifiEvent);
OOP_DECLARE_CLASS(State);

/* ===== Inner state ===== */

typedef enum StateInner {
    STATE_ENTRY,
    STATE_EVENT,
    STATE_EXIT
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
    // This one shall not be overrided
    OOP_IMETHOD(void, State, goTo, State *);
};

/* ===== State base ===== */

OOP_CLASS(State)
{
    OOP_IMPLEMENTS(State);
    const char *name;
    State *parent;
    StateInner inner;
};

/* ctor */
OOP_CTOR(State, State *parent, const char *name);

#endif
