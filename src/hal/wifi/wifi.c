#include "wifi.h"
#include "../dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"

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

static void checkWifiScanResult() {
    WifiScanSt_t st = OOP_CALL(__wifi, getScanStatus);
    if (st != WIFI_SCAN_UNDER_PROCESS) {
        WifiEvent *ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);
        ev->scanStatus = st;
        LOG_TRACE("Wifi status: %d", st);
        ev->apList = &__wifi->apList;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkWifiScanResult);
    }
}

static void startScan() {
    LOG_TRACE("Wifi: start scanning ...");
    getEventloop()->registerChecker(checkWifiScanResult);
    OOP_CALL(__wifi, startScan);
}

OOP_CTOR(Wifi) {
    self->vtable.getApList = getApList;
    self->vtable.getScanStatus = getScanStatus;
    self->vtable.getSignalStrength = getSignalStrength;
    self->startScan = startScan;
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