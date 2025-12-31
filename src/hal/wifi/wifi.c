#include "wifi.h"
#include "../dev/dev.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/wifi_t3Rtos.h"
#endif

static Wifi *wifi;

static WifiApList_t *getApList(Wifi *wifi) {
    return &wifi->apList;
}

static WifiScanSt_t getScanStatus(Wifi *wifi) {
    return wifi->scanSt;
}

static WifiSigStrength_t getSignalStrength(Wifi *wifi) {

}

void Wifi_ctor(Wifi* self) {
    wifi->vtable.getApList = getApList;
    wifi->vtable.getScanStatus = getScanStatus;
    wifi->vtable.getSignalStrength = getSignalStrength;
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