#include "dev.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/dev_t3Rtos.h"
#endif

static Device *device;

void Device_ctor(Device* self, const char* name) {
    self->name = name;
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