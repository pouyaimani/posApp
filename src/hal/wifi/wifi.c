#include "wifi.h"
#include "../dev/dev.h"

Wifi *__wifi;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/wifi_t3Rtos.h"

static void constructT3Rtos() {
    static WifiT3Rtos obj;
    __wifi = (Wifi *)&obj;
    OOP_CALL_CTOR(Wifi, __wifi);
    OOP_CALL_CTOR(WifiT3Rtos, &obj);
}

#endif

static WifiApList_t *getApList(Wifi *self) {
    return &self->apList;
}

static WifiScanSt_t getScanStatus(Wifi *self) {
    return self->scanSt;
}

static WifiSigStrength_t getSignalStrength(Wifi *self) {

}

OOP_CTOR(Wifi) {
    self->vtable.getApList = getApList;
    self->vtable.getScanStatus = getScanStatus;
    self->vtable.getSignalStrength = getSignalStrength;
}

Wifi *getWifi() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce wifi is undefined. Make sure correct device is chosen and its wifi driver is developed.
#endif
    );
    return __wifi;
}