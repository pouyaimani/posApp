#include "dev.h"

Device *__device;
BatteryStat batterySt;
SerialNumber sn;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/dev_t3Rtos.h"

static void constructT3Rtos() {
    static T3Rtos obj;
    __device = (Device *)&obj;
    OOP_CALL_CTOR(Device, __device, "");
    OOP_CALL_CTOR(T3Rtos, &obj, "");
}

#endif

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
    self->vtable.getSN = NULL;
    self->vtable.setAudioVolume = NULL;
    self->vtable.setBrightness = NULL;
}

Device *getDevice() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce is undefined. Make sure correct device is chosen and developed.
#endif
    );
    return __device;
}