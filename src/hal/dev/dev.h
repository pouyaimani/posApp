#ifndef DEV_H_
#define DEV_H_

#include "oop.h"
#include <stdint.h>
#include <stdbool.h>

#define SERIAL_NUMBER_MAX_LEN   50

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

typedef enum BatteryLevel_t {
    DEV_BAT_LEV_LOW,
    DEV_BAT_LEV_1,
    DEV_BAT_LEV_2,
    DEV_BAT_LEV_3
} BatteryLevel_t;

OOP_CLASS(BatteryStat) {
    BatteryLevel_t level;
    bool isChanrging;
    uint16_t percent;
};

OOP_CLASS(SerialNumber) {
    char data[SERIAL_NUMBER_MAX_LEN];
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
    OOP_IMETHOD(BatteryStat*, Device, getBatteryStatus);
    OOP_IMETHOD(void, Device, sleep);
    OOP_IMETHOD(void, Device, reboot);
    OOP_IMETHOD(void, Device, powerOff);
    OOP_IMETHOD(SerialNumber*, Device, getSN);
};

OOP_CLASS(Device) {
    OOP_IMPLEMENTS(Device);
    ModuleExist_t module;
    const char *name;
};

OOP_CTOR(Device, const char* name);

Device *getDevice(void);

#endif