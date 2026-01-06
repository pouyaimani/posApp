#ifndef DEV_H_
#define DEV_H_

#include "oop.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct ModuleExist_t {
    bool wifi;
    bool gprs;
    bool printer;
    bool audio;
    bool scanner;
    bool bt;
} ModuleExist_t;


OOP_CLASS(DateTime) {
    char date[6 + 1];
    char time[6 + 1];
};

OOP_DECLARE_CLASS(Device)

OOP_VTABLE(Device) {
    OOP_IMETHOD(void, Device, init);
    OOP_IMETHOD(unsigned int, Device, getTick);
    OOP_IMETHOD(unsigned int, Device, getMemory, unsigned int);
    OOP_IMETHOD(void, Device, freeMemory, void *);
    OOP_IMETHOD(unsigned int, Device, flushDisplay, int32_t, int32_t, int32_t, int32_t, uint32_t);
    OOP_IMETHOD(void, Device, logOut, const char *, size_t, void *);
    OOP_IMETHOD(DateTime*, Device, getDateTime);
};

OOP_CLASS(Device) {
    OOP_IMPLEMENTS(Device);
    ModuleExist_t module;
    const char *name;
};

OOP_CTOR(Device, const char* name);

Device *getDevice(void);

#endif