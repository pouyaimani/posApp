#ifdef DEVICE_TRENDITT3RTOS

#include "wifi_t3Rtos.h"
#include "posplatform.h"
#include "sdkWifi.h"
#include "dev/dev.h"
#include "logger.h"
#include "utility/arith.h"

static WifiAPInfo *apinfo;
static WifiAPInfo *selectedAp = NULL;

static void libEncodeSsid(char *src, u32 srcLen)
{
    char temp[128] = {0};
    char *tok = "\\x";
    int i = 0, j = 0;

    for (i = 0; i < strlen(src);)
    {
        if (0 == memcmp(src + i, tok, strlen(tok)))
        {
            ascToBcd(&temp[j++], src + i + strlen(tok), 2);
            i += 4;
        }
        else
        {
            temp[j++] = src[i++];
        }
    }

    memset(src, 0, srcLen);
    memcpy(src, temp, j);

    //strip the ""
    if(src[0] == '"' && src[strlen(src) - 1] == '"')
    {
        memset(temp, 0, sizeof(temp));
        snprintf(temp, sizeof(temp), "%s", src);

        memset(src, 0, srcLen);
        memcpy(src, temp + 1, strlen(temp) - 2);
    }
}

static void filterEmptySSID(WifiAPInfo *wifiApInfo, u32 *num)
{
    WifiAPInfo *tempInfo = NULL;
    s32 i = 0, j = 0;
    u32 tempNum = 0;

    if (!wifiApInfo || !num)
    {
        return;
    }

    tempNum = *num;
    tempInfo = sdkSysGetMem(sizeof(WifiAPInfo) * tempNum);
    memset(tempInfo, 0, sizeof(WifiAPInfo) * tempNum);

    memcpy(tempInfo, wifiApInfo, sizeof(WifiAPInfo) * tempNum);

    for (i = 0; i < tempNum; i++)
    {
        libEncodeSsid(tempInfo[i].mSsid, sizeof(tempInfo[i].mSsid));
        if (strlen(tempInfo[i].mSsid) > 0 && tempInfo[i].mSecMode != 0)
        {
            memcpy(&wifiApInfo[j], &tempInfo[i], sizeof(WifiAPInfo));
            j++;
        }
    }

    sdkSysFreeMem(tempInfo);
    *num = (u32)(j);
}

static void sortApInfo(WifiAPInfo *apInfo, int num)
{
    int i, j;
    WifiAPInfo temp = {0};
    wifi_wpa_status_t currentApInfo = {0};

    for (i = 0; i < num - 1; i++)
    {
        for (j = 0; j < num - 1 - i; j++)
        {
            if (apInfo[j].mRssi < apInfo[j + 1].mRssi)
            {
                memcpy(&temp, &apInfo[j], sizeof(WifiAPInfo));
                memcpy(&apInfo[j], &apInfo[j + 1], sizeof(WifiAPInfo));
                memcpy(&apInfo[j + 1], &temp, sizeof(WifiAPInfo));
            }
        }
    }

    sdkWifiIoctl(WIFI_IOCTL_GETWPASTATUS, (uintptr_t)&currentApInfo, 0);
    // put current connection wifi first
    for (i = 1; i < num; i++)
    {
        if ((strcmp(apInfo[i].mSsid, currentApInfo.ssid) != 0))
        {
            continue;
        }

        memset(&temp, 0x00, sizeof(WifiAPInfo));
        memcpy(&temp, &apInfo[i], sizeof(WifiAPInfo));

        memset(&apInfo[i], 0x00, sizeof(WifiAPInfo));
        memcpy(&apInfo[i], &apInfo[0], sizeof(WifiAPInfo));

        memset(&apInfo[0], 0x00, sizeof(WifiAPInfo));
        memcpy(&apInfo[0], &temp, sizeof(WifiAPInfo));
        break;
    }
}

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
    selectedAp = &apinfo[ap->idx];
    WifiErr_t err = translateSdkErr(sdkWifiConnectAP(&apinfo[ap->idx], &passInfo));
    return err;
}

static WifiErr_t disconnect(Wifi* wifi) {
    return translateSdkErr(sdkWifiDisConnectAP());
}

static WifiScanSt_t getScanStatus(Wifi *wifi) {
    uint32_t apNum = 0;
    WIFI_SCAN_AP_STATUS st = sdkWifiGetScanAPStatus(&apNum);
    if (st == WIFI_SCAN_AP_ING) {
        wifi->scanSt = WIFI_SCAN_UNDER_PROCESS;
    } else if (st == WIFI_SCAN_AP_SUCCESS && apNum > 1) {
        int ret = sdkWifiGetScanApInfos(apinfo, &apNum);
        if (ret == SDK_WIFI_OK) {
            wifi->scanSt = WIFI_SCAN_SUCCEED;
            sortApInfo(apinfo, apNum);
            filterEmptySSID(apinfo, &apNum);
            copyWifiAp(wifi, apinfo, apNum);
        } else {
            wifi->scanSt = WIFI_SCAN_FAILED;
        }
    } else {
        wifi->scanSt = WIFI_SCAN_FAILED;
    }
    return wifi->scanSt;
}

static WifiConnectSt_t getConnectStatus(Wifi *wifi) {
    WIFI_CONNECT_AP_STATUS st = sdkWifiGetConnectAPStatus();
    WifiConnectSt_t connectSt;
    switch (st) {
    case WIFI_CONNECT_AP_SUCCESS:
        connectSt = WIFI_CONNECT_SUCCEED;
        break;
    case WIFI_CONNECT_AP_ERR:
        connectSt = WIFI_CONNECT_FAILED;
        break;
    default:
        connectSt = WIFI_CONNECT_UNDER_PROCESS;
        break;
    }
    return connectSt;
}

static WifiSigStrength_t getSignalStrength(Wifi *self) {
    if(sdkWifiGetConnectAPStatus() != WIFI_CONNECT_AP_SUCCESS) {
        return WIFI_SIGNAL_DISCONNECT;
    }
    
    if (selectedAp->mRssi >= -50)
        return WIFI_SIGNAL_STRENGTH_3;
    else if ((selectedAp->mRssi >= -70) && (selectedAp->mRssi < -50))
        return WIFI_SIGNAL_STRENGTH_2;
    else if ((selectedAp->mRssi >= -90) && (selectedAp->mRssi < -70))
        return WIFI_SIGNAL_STRENGTH_1;
    else
        return WIFI_SIGNAL_STRENGTH_0;
}

OOP_CTOR(WifiT3Rtos) {
    self->base.vtable.init = init;
    self->base.vtable.hstartScan = startScan;
    self->base.vtable.hconnect = connect;
    self->base.vtable.hgetScanStatus = getScanStatus;
    self->base.vtable.hdisconnect = disconnect;
    self->base.vtable.hgetConnectStatus = getConnectStatus;
    self->base.vtable.getSignalStrength = getSignalStrength;

    apinfo = GET_MEM(sizeof(WifiAPInfo) * WIFI_AP_LIST_SIZE);
}

#endif
