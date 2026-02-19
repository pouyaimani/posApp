#ifdef DEVICE_TRENDITT3RTOS

#include "wifi_t3Rtos.h"
#include "posplatform.h"
#include "sdkWifi.h"
#include "dev/dev.h"
#include "logger.h"

static WifiAPInfo apinfo[WIFI_AP_LIST_SIZE];

static WifiErr_t translateSdkErr(int err) {
    WifiErr_t wifiErr;
    switch (err) {
    case SDK_WIFI_OK:
        wifiErr = WIFI_ERR_OK;
        break;
    case SDK_WIFI_ERR_INPUT :
        wifiErr = WIFI_ERR_INPUT;
        break;
    case SDK_WIFI_ERR_TIMEOUT:
        wifiErr = WIFI_ERR_TIME_OUT;
        break;
    default:
        wifiErr = WIFI_ERR_NOK;
        break;
    }
    return wifiErr;
}

static void copyWifiAp(Wifi* wifi, WifiAPInfo *apInfo, uint32_t num) {
    wifi->apList.size = num;
    LOG_TRACE("Copeing wifi aps, num = %d", num);
    for (uint32_t i = 0; i < num ; i++) {
        memcpy(wifi->apList.list[i].essid, apinfo[i].mSsid,
                 sizeof(wifi->apList.list[i].essid));
        wifi->apList.list[i].idx = i;
    }
}

static void init(Wifi* wifi) {
    sdkWifiOpen();
}

static WifiErr_t startScan(Wifi* wifi) {
    wifi->scanSt = WIFI_SCAN_FAILED;
    WifiErr_t err = translateSdkErr(sdkWifiStartScanAP(apinfo, WIFI_AP_LIST_SIZE));
    if (err == WIFI_ERR_OK) {
        wifi->scanSt = WIFI_SCAN_UNDER_PROCESS;
    }
    return err;
}

static WifiErr_t connect(Wifi* wifi, WifiApInfo_t* ap, char *pass) {
    WifiAPPasswordInfo passInfo;
    memset(passInfo.mWpaPassword, 0, sizeof(passInfo.mWpaPassword));
    snprintf(passInfo.mWpaPassword, sizeof(passInfo.mWpaPassword), "%s", pass);
    LOG_TRACE("wifi wssid = %s", apinfo[ap->idx].mSsid);
    LOG_TRACE("wifi pass = %s", passInfo.mWpaPassword);
    passInfo.mSecMode = apinfo[ap->idx].mSecMode;
    WifiErr_t err = translateSdkErr(sdkWifiConnectAP(&apinfo[ap->idx], &passInfo));
    LOG_TRACE("hal connect to wifi err = %d", err);
    return err;
}

static WifiErr_t disconnect(Wifi* wifi) {
    return translateSdkErr(sdkWifiDisConnectAP());
}

static WifiScanSt_t getScanStatus(Wifi *wifi) {
    uint32_t apNum = 0;
    WIFI_SCAN_AP_STATUS st = sdkWifiGetScanAPStatus(&apNum);
    switch (st) {
    case WIFI_SCAN_AP_SUCCESS:
        wifi->scanSt = WIFI_SCAN_SUCCEED;
        copyWifiAp(wifi, apinfo, apNum);
        break;
    case WIFI_SCAN_AP_ERR:
        wifi->scanSt = WIFI_SCAN_FAILED;
        break;
    default:
        wifi->scanSt = WIFI_SCAN_UNDER_PROCESS;
        break;
    }
    return wifi->scanSt;
}

static WifiConnectSt_t getConnectStatus(Wifi *wifi) {
    WIFI_CONNECT_AP_STATUS st = sdkWifiGetConnectAPStatus();
    switch (st) {
    case WIFI_CONNECT_AP_SUCCESS:
        wifi->connectSt = WIFI_CONNECT_SUCCEED;
        break;
    case WIFI_CONNECT_AP_ERR:
        wifi->connectSt = WIFI_CONNECT_FAILED;
        break;
    default:
        wifi->connectSt = WIFI_CONNECT_UNDER_PROCESS;
        break;
    }
    return wifi->connectSt;
}

OOP_CTOR(WifiT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.hstartScan = startScan;
    self->base.vtable.hconnect = connect;
    self->base.vtable.hgetScanStatus = getScanStatus;
    self->base.vtable.hdisconnect = disconnect;
    self->base.vtable.hgetConnectStatus = getConnectStatus;
}

#endif
