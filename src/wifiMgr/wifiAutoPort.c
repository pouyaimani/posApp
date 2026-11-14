#include "wifiAutoPort.h"
#include "wifiPwdMgr.h"
#include "wifi/wifi.h"
#include "logger.h"
#include <string.h>
#include "network/network.h"

#include <string.h>

/*
 * Persistent buffers used during an automatic
 * Wi-Fi connection attempt.
 */
static WifiApInfo_t g_autoAp;
static char         g_autoPassword[WIFI_PM_PASSWORD_CAPACITY];

bool wifiAutoPortActivateRoute(void* ctx) {
    (void)ctx;

    WifiConnectSt_t status = OOP_CALL(wifi(), getConnectStatus);

    NetRoute_t route = OOP_CALL(network(), getRoute);

    LOG_WAC("WiFi auto: activating route, "
            "connectStatus=%d, route=%d",
            (int)status, (int)route);

    if (status != WIFI_CONNECT_SUCCEED) {
        LOG_ERROR("WiFi auto: WiFi is not connected");
        return false;
    }

    if (route != NET_ROUTE_WIFI) {
        LOG_ERROR("WiFi auto: WiFi is not selected route");
        return false;
    }

    return true;
}

void wifiAutoPortFinished(void* ctx, WifiAutoOutcome outcome,
                          WifiPmResult persistence) {
    (void)ctx;

    LOG_WAC("WiFi auto: finished, outcome=%d, persistence=%d", (int)outcome,
            (int)persistence);

    switch (outcome) {

    case WIFI_AUTO_CONNECTED:
        LOG_WAC("WiFi auto: connected successfully");

        if (persistence != WIFI_PM_OK) {
            LOG_ERROR("WiFi auto: password save failed=%d", (int)persistence);
        }
        break;

    case WIFI_AUTO_NO_SAVED_AP:
        LOG_WAC("WiFi auto: no saved AP found");
        break;

    case WIFI_AUTO_ALL_FAILED:
        LOG_ERROR("WiFi auto: all saved APs failed");
        break;

    case WIFI_AUTO_SCAN_FAILED:
        LOG_ERROR("WiFi auto: scanning failed");
        break;

    case WIFI_AUTO_HAL_ERROR:
        LOG_ERROR("WiFi auto: HAL operation failed");
        break;

    case WIFI_AUTO_TIMEOUT:
        LOG_ERROR("WiFi auto: operation timeout");
        break;

    case WIFI_AUTO_CANCELLED:
        LOG_WAC("WiFi auto: operation cancelled");
        break;

    default:
        LOG_ERROR("WiFi auto: unknown outcome=%d", (int)outcome);
        break;
    }

    /*
     * These outcomes indicate that no previous connection
     * attempt should still be using the password buffer.
     *
     * On timeout or cancellation, the HAL may still be
     * operating, so its buffers must be released only
     * after the operation has been aborted/drained.
     */
    if (outcome == WIFI_AUTO_CONNECTED || outcome == WIFI_AUTO_NO_SAVED_AP ||
        outcome == WIFI_AUTO_ALL_FAILED || outcome == WIFI_AUTO_SCAN_FAILED ||
        (outcome == WIFI_AUTO_CANCELLED && wifiAutoConnector()->canHandoff())) {

        wifiPmSecureZero(g_autoPassword, sizeof(g_autoPassword));
    }
}

bool wifiAutoPortRequestConnect(void* ctx, uint16_t apIndex,
                                const char* password) {
    (void)ctx;

    if (password == NULL || apIndex >= wifi()->apList.size ||
        apIndex >= LEN_WIFI_AP_LIST) {
        LOG_ERROR("WiFi auto: invalid connect arguments");
        return false;
    }

    /* Validate password length without truncating it. */
    size_t len = 0;

    while (len < sizeof(g_autoPassword) && password[len] != '\0') {
        len++;
    }

    if (len >= sizeof(g_autoPassword)) {
        LOG_ERROR("WiFi auto: password too long");
        return false;
    }

    /* Preserve AP and password during connection. */
    g_autoAp = wifi()->apList.list[apIndex];

    wifiPmSecureZero(g_autoPassword, sizeof(g_autoPassword));

    memcpy(g_autoPassword, password, len + 1u);

    LOG_WAC("WiFi auto: connecting to AP index=%u", (unsigned)apIndex);

    WifiErr_t err = wifi()->connect(&g_autoAp, g_autoPassword);

    if (err != WIFI_ERR_OK) {
        LOG_ERROR("WiFi auto: connect request failed=%d", (int)err);
        return false;
    }

    return true;
}

bool wifiAutoPortRequestDisconnect(void* ctx) {
    (void)ctx;

    LOG_WAC("WiFi auto: requesting disconnect");

    WifiErr_t err = wifi()->disconnect();

    if (err != WIFI_ERR_OK) {
        LOG_ERROR("WiFi auto: disconnect request failed, err=%d", (int)err);
        return false;
    }

    LOG_WAC("WiFi auto: disconnect request accepted");

    return true;
}

/* Request a Wi-Fi scan. */
bool wifiAutoPortStartScan(void* ctx) {
    (void)ctx;

    WifiErr_t err = wifi()->startScan();

    LOG_WAC("WiFi auto: start scan, result=%d", (int)err);

    return err == WIFI_ERR_OK;
}

/* Return the number of scanned access points. */
uint16_t wifiAutoPortScanCount(void* ctx) {
    (void)ctx;

    uint16_t count = wifi()->apList.size;

    if (count > LEN_WIFI_AP_LIST)
        count = LEN_WIFI_AP_LIST;

    if (count > WIFI_PM_MAX_SCAN_APS)
        count = WIFI_PM_MAX_SCAN_APS;

    return count;
}

/* Return one scanned access point to WifiPwdMgr. */
bool wifiAutoPortScanGet(void* ctx, uint16_t index, WifiPmAccessPoint* out) {
    (void)ctx;

    if (out == NULL || index >= wifiAutoPortScanCount(NULL)) {
        return false;
    }

    const WifiApInfo_t* ap = &wifi()->apList.list[index];

    out->ssid     = ap->essid;
    out->bssid    = ap->mac;
    out->security = ap->secMode;

    return true;
}

bool wifiAutoPortFill(WifiAutoPort* port) {
    if (!port) {
        return false;
    }
    port->ctx               = NULL;
    port->startScan         = wifiAutoPortStartScan;
    port->scanCount         = wifiAutoPortScanCount;
    port->scanGet           = wifiAutoPortScanGet;
    port->requestDisconnect = wifiAutoPortRequestDisconnect;
    port->requestConnect    = wifiAutoPortRequestConnect;
    port->activateRoute     = wifiAutoPortActivateRoute;
    port->finished          = wifiAutoPortFinished;
    return true;
}

void wifiAutoPortHandleEvent(const WifiEvent* ev) {
    if (ev == NULL)
        return;

    WifiAutoConnector* ac = wifiAutoConnector();

    if (!ac->isReady())
        return;

    WifiAutoStage stage = ac->getStage();

    switch (ev->type) {

    case WIFI_EV_SCAN:
        if (stage == WIFI_AUTO_SCANNING) {
            if (ev->scanStatus == WIFI_SCAN_SUCCEED)
                ac->onScanComplete(true);
            else if (ev->scanStatus == WIFI_SCAN_FAILED)
                ac->onScanComplete(false);
        }
        break;

    case WIFI_EV_DISCONNECT:
        if (stage == WIFI_AUTO_WAIT_DISCONNECT)
            ac->onDisconnected();
        break;

    case WIFI_EV_CONNECT:
        if (stage == WIFI_AUTO_WAIT_CONNECT) {
            if (ev->connectStatus == WIFI_CONNECT_SUCCEED)
                ac->onConnectResult(true);
            else if (ev->connectStatus == WIFI_CONNECT_FAILED)
                ac->onConnectResult(false);
        }
        break;

    default:
        break;
    }
}