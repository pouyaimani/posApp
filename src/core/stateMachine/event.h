#ifndef EVENT_H
#define EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "oop.h"
#include "eventIds.h"

OOP_DECLARE_CLASS(State);
OOP_DECLARE_CLASS(Event);

typedef Event* (*EventFactoryFn)(void);

OOP_VTABLE(Event) {
    OOP_IMETHOD(void, Event, dispatch, State* state);
    OOP_IMETHOD(void, Event, dispatchTo, State* state);
};

OOP_CLASS(Event) {
    OOP_IMPLEMENTS(Event);

    State*  target;
    EventId id;
};

OOP_CTOR(Event);

/* Registry API */
bool   eventRegister(EventId id, EventFactoryFn factory);
Event* createEvent(EventId id);

/* Base event initialization */
void eventInit(void);

/* Event dispatch */
#define DISPATCH_EVENT(event)                                                  \
    ((Event*)(event))->vtable.dispatch((Event*)(event), NULL)

#endif