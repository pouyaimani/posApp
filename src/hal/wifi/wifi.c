#include "wifi.h"
#include "sys/sys.h"
#include "eventloop.h"
#include "eventAll.h"
#include "logger.h"
#include "error.h"

Wifi* __wifi;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/wifi_t3Rtos.h"

static void constructT3Rtos() {
    static WifiT3Rtos obj;
    __wifi = (Wifi*)&obj;
    OOP_CALL_CTOR(Wifi, __wifi);
    OOP_CALL_CTOR(WifiT3Rtos, &obj);
}

#endif

static WifiApList_t* getApList(Wifi* self) { return &self->apList; }

static void checkWifiScanResult() {
    WifiScanSt_t st = OOP_CALL(__wifi, hgetScanStatus);
    if (st != WIFI_SCAN_UNDER_PROCESS) {
        WifiEvent* ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);
        RETURN_IF_NULL(ev, ;);
        ev->type       = WIFI_EV_SCAN;
        ev->scanStatus = st;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkWifiScanResult);
    }
}

static WifiErr_t startScan(void) {
    WifiErr_t err;

    LOG_TRACE("Wifi: start scanning ...");

    err = OOP_CALL(__wifi, hstartScan);

    if (err != WIFI_ERR_OK) {
        LOG_ERROR("Wifi: start scan failed, err=%d", err);
        return err;
    }

    getEventloop()->registerChecker(checkWifiScanResult);

    return WIFI_ERR_OK;
}

static WifiErr_t scanInBg() { return OOP_CALL(__wifi, hstartScan); }

static void checkWifiConnectResult(void) {
    WifiConnectSt_t st = OOP_CALL(__wifi, getConnectStatus);

    if (st == WIFI_CONNECT_UNDER_PROCESS)
        return;

    WifiEvent* ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);
    RETURN_IF_NULL(ev, ;);

    ev->type          = WIFI_EV_CONNECT;
    ev->connectStatus = st;

    DISPATCH_EVENT(ev);

    getEventloop()->unregisterChecker(checkWifiConnectResult);
}

static void checkWifiDisconnectResult(void) {
    bool disconnected = OOP_CALL(__wifi, isDisconnected);

    if (!disconnected)
        return;

    LOG_TRACE("Wifi: disconnection confirmed");

    /* Stop polling before dispatching the event. */
    getEventloop()->unregisterChecker(checkWifiDisconnectResult);

    WifiEvent* ev = (WifiEvent*)createEvent(SM_EVENT_WIFI);

    if (ev == NULL) {
        LOG_ERROR("Wifi: cannot create disconnect event");
        return;
    }

    ev->type = WIFI_EV_DISCONNECT;

    DISPATCH_EVENT(ev);
}

static WifiErr_t connect(WifiApInfo_t* apinfo, char* password) {
    WifiErr_t err;

    if (apinfo == NULL || password == NULL)
        return WIFI_ERR_INPUT;

    err = OOP_CALL(__wifi, hconnect, apinfo, password);

    if (err == WIFI_ERR_OK) {
        getEventloop()->registerChecker(checkWifiConnectResult);
    }

    return err;
}

static WifiErr_t disconnect(void) {
    WifiErr_t err;

    LOG_TRACE("Wifi: requesting disconnect");

    err = OOP_CALL(__wifi, hdisconnect);

    if (err != WIFI_ERR_OK) {
        LOG_ERROR("Wifi: disconnect request failed, err=%d", (int)err);
        return err;
    }

    /*
     * Disconnect requests must not be interpreted
     * as connection attempts.
     */
    getEventloop()->unregisterChecker(checkWifiConnectResult);

    getEventloop()->registerChecker(checkWifiDisconnectResult);

    return WIFI_ERR_OK;
}

OOP_CTOR(Wifi) {
    memset(&self->vtable, 0, sizeof(self->vtable));
    self->getApList  = getApList;
    self->startScan  = startScan;
    self->connect    = connect;
    self->disconnect = disconnect;
    self->scanInBg   = scanInBg;

    wifiEventRegister();
}

Wifi* wifi() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Deivce wifi is undefined. Make sure correct device is chosen and its wifi driver is developed.
#endif
    );
    return __wifi;
}