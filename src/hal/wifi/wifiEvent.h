#ifndef WIFI_EVENT_H
#define WIFI_EVENT_H

#include "event.h"

/* ===== Wifi ===== */

typedef enum WifiScanSt_t {
    WIFI_SCAN_FAILED,
    WIFI_SCAN_SUCCEED,
    WIFI_SCAN_UNDER_PROCESS
} WifiScanSt_t;

typedef enum WifiConnectSt_t {
    WIFI_CONNECT_FAILED,
    WIFI_CONNECT_SUCCEED,
    WIFI_CONNECT_UNDER_PROCESS
} WifiConnectSt_t;

typedef enum WifiEvType_t {
    WIFI_EV_CONNECT,
    WIFI_EV_SCAN,
} WifiEvType_t;

OOP_DECLARE_CLASS(WifiApList_t);

OOP_CLASS(WifiEvent) {
    OOP_EXTENDS(Event);
    WifiScanSt_t    scanStatus;
    WifiConnectSt_t connectStatus;
    WifiEvType_t    type;
    WifiApList_t*   apList;
};

/* ctor */
OOP_CTOR(WifiEvent);

void wifiEventRegister(void);

#endif