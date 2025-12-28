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
        static T3Rtos t3Rtos;
        T3Rtos_ctor(&t3Rtos, "");
        device = (Device*)&t3Rtos;
#endif
    );
    return device;
}