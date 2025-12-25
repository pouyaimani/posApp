#ifndef DEV_H_
#define DEV_H_

#include "oop.h"

OOP_DECLARE_CLASS(Device)

OOP_VTABLE(Device) {
    OOP_IMETHOD(Device, init);
};

OOP_CLASS(Device) {
    OOP_IMPLEMENTS(Device);
    const char *name;
};

OOP_CTOR(Device, const char* name);

Device *getDevice(void);

#endif