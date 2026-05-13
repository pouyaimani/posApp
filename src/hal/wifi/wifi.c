#include "wifi.h"
#include "sys/sys.h"
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

static void checkWifiScanResult() {
    WifiScanSt_t st = OOP_CALL(__wifi, hgetScanStatus);
    if (st != WIFI_SCAN_UNDER_PROCESS) {
        WifiEvent *ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);
        ev->scanStatus = st;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkWifiScanResult);
    }
}

static void startScan() {
    LOG_TRACE("Wifi: start scanning ...");
    getEventloop()->registerChecker(checkWifiScanResult);
    OOP_CALL(__wifi, hstartScan);
}

static void checkWifiConnectResult() {
    WifiConnectSt_t st = OOP_CALL(__wifi, getConnectStatus);
    if (st != WIFI_SCAN_UNDER_PROCESS) {
        WifiEvent *ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);
        ev->connectStatus = st;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkWifiConnectResult);
    }
}

static WifiErr_t connect(WifiApInfo_t *apinfo, char *password) {
    getEventloop()->registerChecker(checkWifiConnectResult);
    OOP_CALL(__wifi, hconnect, apinfo, password);
}

static WifiErr_t disconnect() {
    getEventloop()->registerChecker(checkWifiConnectResult);
    OOP_CALL(__wifi, hdisconnect);
}

OOP_CTOR(Wifi) {
    self->vtable.hconnect = NULL;
    self->vtable.hdisconnect = NULL;
    self->vtable.hstartScan = NULL;
    self->vtable.init = NULL;
    self->vtable.hgetScanStatus = NULL;
    self->vtable.getConnectStatus = NULL;
    self->vtable.getSignalStrength = NULL;

    self->getApList = getApList;
    self->startScan = startScan;
    self->connect = connect;
    self->disconnect = disconnect;
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