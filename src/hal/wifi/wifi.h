#ifndef WIFI_H_
#define WIFI_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"
#include "event.h"

#ifndef WIFI_AP_LIST_SIZE
#error Wifi AP info list size is undefined.
#endif

#define WIFI_PASS_SIZE  32

typedef enum WifiErr_t {
    WIFI_ERR_OK,
    WIFI_ERR_NOK,
    WIFI_ERR_INPUT,
    WIFI_ERR_TIME_OUT,
    WIFI_ERR_CONNECT_FAILED
} WifiErr_t;

typedef enum WifiSigStrenght_t {
    WIFI_SIGNAL_STRENGTH_0,
    WIFI_SIGNAL_STRENGTH_1,
    WIFI_SIGNAL_STRENGTH_2,
    WIFI_SIGNAL_STRENGTH_3
} WifiSigStrength_t;

typedef struct WifiApInfo_t {
    char essid[64];     /* AP name */
    int idx;
} WifiApInfo_t;

typedef struct WifiApList_t {
    WifiApInfo_t list[WIFI_AP_LIST_SIZE];
    uint16_t size;
} WifiApList_t;

OOP_DECLARE_CLASS(Wifi)

OOP_VTABLE(Wifi) {
    OOP_IMETHOD(void, Wifi, init);
    OOP_IMETHOD(WifiErr_t, Wifi, hconnect, WifiApInfo_t *, char *);
    OOP_IMETHOD(WifiErr_t, Wifi, hdisconnect);
    OOP_IMETHOD(void, Wifi, hstartScan);
    OOP_IMETHOD(WifiScanSt_t, Wifi, hgetScanStatus);
    OOP_IMETHOD(WifiScanSt_t, Wifi, hgetConnectStatus);
};

OOP_CLASS(Wifi) {
    OOP_IMPLEMENTS(Wifi);
    OOP_METHOD(void, startScan);
    WifiSigStrength_t signalStrength;
    OOP_METHOD(WifiSigStrength_t, getSignalStrength);
    OOP_METHOD(WifiApList_t, getApList);
    OOP_METHOD(WifiErr_t, connect, WifiApInfo_t *, char *);
    OOP_METHOD(WifiErr_t,  disconnect);
    WifiApList_t apList;
    WifiScanSt_t scanSt;
    WifiConnectSt_t connectSt;
};

OOP_CTOR(Wifi);

Wifi *getWifi(void);

#define WIFI_INIT()  OOP_CALL(getWifi(), init)
#define WIFI_START_SCAN() getWifi()->startScan()

#endif