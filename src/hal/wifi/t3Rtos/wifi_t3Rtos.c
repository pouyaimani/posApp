#ifdef DEVICE_TRENDITT3RTOS

#include "wifi_t3Rtos.h"
#include "posplatform.h"
#include "sdkWifi.h"
#include "../../dev/dev.h"

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
    for (uint32_t i = 0; i < num ; i++) {
        wifi->apList.list->secMode = apinfo->mSecMode;
        memcpy(wifi->apList.list->essid, apinfo->mSsid,
                 sizeof(wifi->apList.list->essid));
        memcpy(wifi->apList.list->bssid, apinfo->mMac,
                 sizeof(wifi->apList.list->essid));
        wifi->apList.list->rssi = apinfo->mRssi;
        wifi->apList.list->channel = apinfo->mChannel;
    }
}

static void init(Wifi* wifi) {
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
    WifiErr_t err = WIFI_ERR_CONNECT_FAILED;
    for (uint16_t i = 0; i < wifi->apList.size ; i++) {
        if (strcmp(ap->essid, apinfo[i].mSsid) == 0) {
             WifiAPPasswordInfo passInfo;
            memcpy(passInfo.mWpaPassword, pass,
                 sizeof(pass));
            // TODO
            passInfo.mSecMode = WIFI_SEC_UNSEC;
            err = translateSdkErr(sdkWifiConnectAP(&apinfo[i], &passInfo));
        }
    }
    return err;
}

static WifiErr_t disconnect(Wifi* wifi) {
    return translateSdkErr(sdkWifiDisConnectAP());
}

static WifiScanSt_t getScanStatus(Wifi *wifi) {
    uint32_t apNum = 0;
    WIFI_SCAN_AP_STATUS st = sdkWifiGetScanAPStatus(&apNum);
    switch (st) {
    case WIFI_CONNECT_AP_SUCCESS:
        wifi->scanSt = WIFI_SCAN_SUCCEED;
        copyWifiAp(wifi, apinfo, apNum);
        break;
    case WIFI_CONNECT_AP_ERR :
        wifi->scanSt = WIFI_SCAN_FAILED;
        break;
    default:
        wifi->scanSt = WIFI_SCAN_UNDER_PROCESS;
        break;
    }
    return wifi->scanSt;
}

void WifiT3Rtos_ctor(WifiT3Rtos* self) {
    self->base.vtable.init = init;
    self->base.vtable.startScan = startScan;
    self->base.vtable.connect = connect;
    self->base.vtable.getScanStatus = getScanStatus;  
}

#endif
