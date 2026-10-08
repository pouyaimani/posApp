#ifndef PED_EVENT_H
#define PED_EVENT_H

#include "event.h"

typedef enum PedEventType_t {
    PED_DIGIT = 0,
    PED_ENTER,
    PED_CANCEL,
    PED_CLEAR,
    PED_DUMMY
} PedEventType_t;

OOP_CLASS(PedEvent) {
    OOP_EXTENDS(Event);
    PedEventType_t type;
};

/* ctor */
OOP_CTOR(PedEvent);

void pedEventRegister(void);

#endif