#ifndef TO_EVENT_H
#define TO_EVENT_H

#include "event.h"

/* ===== TimeOut ===== */

OOP_CLASS(TimeOutEvent) { OOP_EXTENDS(Event); };

/* ctor */
OOP_CTOR(TimeOutEvent);

void toEventRegister(void);

#endif