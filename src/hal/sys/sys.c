#include "sys.h"

System *__sys;
BatteryStat batterySt;
char serialNumber[SERIAL_NUMBER_MAX_LEN];

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/sys_t3Rtos.h"

static void constructT3Rtos() {
    static T3Rtos obj;
    __sys = (System *)&obj;
    OOP_CALL_CTOR(System, __sys, "");
    OOP_CALL_CTOR(T3Rtos, &obj, "");
}

#endif

OOP_CTOR(System, const char* name) {
    self->name = name;
    self->vtable.flushDisplay = NULL;
    self->vtable.freeMemory = NULL;
    self->vtable.getDateTime = NULL;
    self->vtable.getMemory = NULL;
    self->vtable.getTick = NULL;
    self->vtable.init = NULL;
    self->vtable.logOut = NULL;
    self->vtable.getBatteryStatus = NULL;
    self->vtable.sleep = NULL;
    self->vtable.reboot = NULL;
    self->vtable.powerOff = NULL;
    self->vtable.getSN = NULL;
    self->vtable.setVolume = NULL;
    self->vtable.setBrightness = NULL;
    self->vtable.beepOnce = NULL;
    self->vtable.getVolume = NULL;
    self->vtable.getBrightness= NULL;
    self->vtable.setDateTime= NULL;
    self->vtable.getName= NULL;
    self->vtable.getCode= NULL;
}

System *sys() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce is undefined. Make sure correct device is chosen and developed.
#endif
    );
    return __sys;
}