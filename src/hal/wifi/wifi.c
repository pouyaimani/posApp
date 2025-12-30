#include "wifi.h"
#include "../dev/dev.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/wifi_t3Rtos.h"
#endif

static Wifi *wifi;

void Wifi_ctor(Wifi* self) {
}

Wifi *getWifi() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    DEVICE_REGISTER(Wifi, WifiT3Rtos, wifi)
#else
#error Deivce wifi is undefined. Make sure correct device is chosen and its wifi driver is developed.
#endif
    );
    return wifi;
}