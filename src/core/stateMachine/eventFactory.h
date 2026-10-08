#ifndef EVENT_FACTORY_H
#define EVENT_FACTORY_H

#include "eventAll.h"
#include "sys/sys.h"

#define EVENT_DEFINE_FACTORY(Type)                                             \
    static Event* Type##_factory(void) {                                       \
        Type* ev = MEM_ALLOC(sizeof(Type));                                    \
        RETURN_VALUE_IF_NULL(ev, ;, NULL);                                     \
                                                                               \
        OOP_CALL_CTOR(Event, (Event*)ev);                                      \
        OOP_CALL_CTOR(Type, ev);                                               \
                                                                               \
        return (Event*)ev;                                                     \
    }

#define EVENT_REGISTER(Id, Type) eventRegister((Id), Type##_factory)

#endif