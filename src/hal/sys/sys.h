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
    bool dialup;
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

OOP_DECLARE_CLASS(System)

OOP_VTABLE(System) {
    OOP_IMETHOD(void, System, init);
    OOP_IMETHOD(unsigned int, System, getTick);
    OOP_IMETHOD(unsigned int, System, getMemory, unsigned int);
    OOP_IMETHOD(void, System, freeMemory, void *);
    OOP_IMETHOD(unsigned int, System, flushDisplay, int32_t, int32_t, int32_t, int32_t, uint8_t*);
    OOP_IMETHOD(DateTime*, System, getDateTime);
    OOP_IMETHOD(uint32_t, System, getDate);
    OOP_IMETHOD(uint32_t, System, getTime);
    OOP_IMETHOD(uint64_t, System, getPackedDateTime);
    OOP_IMETHOD(void, System, setDateTime, DateTime *);
    OOP_IMETHOD(void, System, logOut, const char *, size_t, void *);
    OOP_IMETHOD(BatteryStat*, System, getBatteryStatus);
    OOP_IMETHOD(void, System, sleep);
    OOP_IMETHOD(void, System, reboot);
    OOP_IMETHOD(void, System, powerOff);
    OOP_IMETHOD(const char*, System, getSN);
    OOP_IMETHOD(const char*, System, getCode);
    OOP_IMETHOD(const char*, System, getName);
    OOP_IMETHOD(void, System, setVolume, int);
    OOP_IMETHOD(void, System, setBrightness, int);
    OOP_IMETHOD(int, System, getVolume);
    OOP_IMETHOD(int, System, getBrightness);
    OOP_IMETHOD(void, System, beepOnce);
};

OOP_CLASS(System) {
    OOP_IMPLEMENTS(System);
    ModuleExist_t module;
    const char *name;
    int maxBright;
    int maxSound;
};

OOP_CTOR(System, const char* name);

System *sys(void);

#define MEM_ALLOC(size) OOP_CALL(sys(), getMemory, size)
#define MEM_FREE(mem) OOP_CALL(sys(), freeMemory, mem)
#define GET_TICK() OOP_CALL(sys(), getTick)

#endif