#ifdef DEVICE_TRENDITT3RTOS

#include "wifi_t3Rtos.h"
#include "posplatform.h"
#include "sdkWifi.h"

static void init(WifiT3Rtos* wifi) {
}

void WifiT3Rtos_ctor(WifiT3Rtos* self) {
    self->base.vtable.init = init;
}

#endif
