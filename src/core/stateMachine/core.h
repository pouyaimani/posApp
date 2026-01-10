#ifndef CORE_H
#define CORE_H

#include "oop.h"
#include <stddef.h>

OOP_DECLARE_CLASS(State);
OOP_DECLARE_CLASS(Event);

/* Callback type */
typedef void (*CoreCallback)(void);

OOP_CLASS(Core)
{
    OOP_METHOD(void, exec);
    OOP_METHOD(void, init, State *);
    OOP_METHOD(void, runCycle);
    OOP_METHOD(void, raiseEvent, Event *);
    OOP_METHOD(void, goTo, State *);
    OOP_METHOD(void, registerCallback, CoreCallback cb);
    State *current;
    State *next;

    Event *queue[16];
    size_t qsize;
    CoreCallback callbacks[16];
    size_t cbSize;
};

OOP_CTOR(Core);

/* singleton */
Core *getSmCore(void);

#endif
