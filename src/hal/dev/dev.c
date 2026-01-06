#include "dev.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/dev_t3Rtos.h"
#endif

static Device *device;
BatteryStat batterySt;

OOP_CTOR(Device, const char* name) {
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
}

Device *getDevice() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    DEVICE_REGISTER(Device, T3Rtos, device, "")
#else
#error Deivce is undefined. Make sure correct device is chosen and developed.
#endif
    );
    return device;
}