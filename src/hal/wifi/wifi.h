#ifndef WIFI_H_
#define WIFI_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"

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
	char bssid[18];	    /* MAC address */
	int channel;		/* Channel */
	int mode;		    /* Connection mode, 0:Station; 1:IBSS */
	int rssi;		    /* Signal value */
	int authMode;		/* Authentication modes */
	int secMode;		/* Encryption mode, NONE,WEP,TKIP,CCMP */
} WifiApInfo_t;

typedef struct WifiApList_t {
    WifiApInfo_t list[WIFI_AP_LIST_SIZE];
    uint16_t size;
} WifiApList_t;

typedef enum WifiScanSt_t {
    WIFI_SCAN_UNDER_PROCESS,
    WIFI_SCAN_SUCCEED,
    WIFI_SCAN_FAILED
} WifiScanSt_t;

OOP_DECLARE_CLASS(Wifi)

OOP_VTABLE(Wifi) {
    OOP_IMETHOD(void, Wifi, init);
    OOP_IMETHOD(WifiSigStrength_t, Wifi, getSignalStrength);
    OOP_IMETHOD(WifiErr_t, Wifi, startScan);
    OOP_IMETHOD(WifiScanSt_t, Wifi, getScanStatus);
    OOP_IMETHOD(WifiErr_t, Wifi, connect, WifiApInfo_t *, char *);
    OOP_IMETHOD(WifiApList_t, Wifi, getApList);
    OOP_IMETHOD(WifiErr_t, Wifi, disconnect);
};

OOP_CLASS(Wifi) {
    OOP_IMPLEMENTS(Wifi);
    WifiSigStrength_t signalStrength;
    WifiApList_t apList;
    WifiScanSt_t scanSt;
};

OOP_CTOR(Wifi);

Wifi *getWifi(void);

#endif