#include "wifiAutoConnector.h"

#include <string.h>

/* The class is stateless by design; the runtime is a private singleton. */
typedef struct {
    WifiAutoPort    port;
    uint8_t         tried[(WIFI_PM_MAX_SCAN_APS + 7u) / 8u];
    WifiPmCandidate current;
    uint16_t        apCount;
    uint16_t        tries;
    WifiAutoStage   stage;
    bool            initialized;
} WifiAutoRuntime;

static WifiAutoRuntime   g_auto;
static WifiAutoConnector __wifiAutoConnector;

static void acClearCurrent(void) {
    wifiPmSecureZero(&g_auto.current, sizeof(g_auto.current));
}

static void acFinish(WifiAutoOutcome outcome, WifiPmResult persisted) {
    if (g_auto.stage == WIFI_AUTO_IDLE || g_auto.stage == WIFI_AUTO_FINISHED)
        return;

    g_auto.stage = WIFI_AUTO_FINISHED;
    acClearCurrent();
    if (g_auto.port.finished != NULL)
        g_auto.port.finished(g_auto.port.ctx, outcome, persisted);
}

static bool acWasTried(uint16_t index) {
    return (g_auto.tried[index / 8u] & (uint8_t)(1u << (index % 8u))) != 0u;
}

static void acMarkTried(uint16_t index) {
    g_auto.tried[index / 8u] |= (uint8_t)(1u << (index % 8u));
}

static void acAttemptNext(void) {
    WifiPwdMgr* pm = wifiPwdMgr();
    acClearCurrent();
    if (!pm->pickCandidate(g_auto.port.scanGet, g_auto.port.ctx, g_auto.apCount,
                           g_auto.tried, sizeof(g_auto.tried),
                           &g_auto.current)) {
        acFinish(g_auto.tries == 0u ? WIFI_AUTO_NO_SAVED_AP
                                    : WIFI_AUTO_ALL_FAILED,
                 WIFI_PM_OK);
        return;
    }
    /* An AP index is attempted once per scan even if multiple BSSIDs share
     * the same SSID. Do not reset this bitmap between failures. */
    if (acWasTried(g_auto.current.apIndex)) {
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK);
        return;
    }
    acMarkTried(g_auto.current.apIndex);
    ++g_auto.tries;
    g_auto.stage = WIFI_AUTO_WAIT_DISCONNECT;

    /* Do not connect until confirmed disconnected. If the HAL is synchronous,
     * application must queue a separate disconnect-completed event. */
    if (!g_auto.port.requestDisconnect(g_auto.port.ctx))
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK);
}

static bool acInit(const WifiAutoPort* port) {
    if (port == NULL || !wifiPwdMgr()->isReady() || port->startScan == NULL ||
        port->scanCount == NULL || port->scanGet == NULL ||
        port->requestDisconnect == NULL || port->requestConnect == NULL ||
        port->activateRoute == NULL)
        return false;

    /* Do not replace active HAL callbacks during an operation. */
    if (g_auto.initialized && g_auto.stage != WIFI_AUTO_IDLE &&
        g_auto.stage != WIFI_AUTO_FINISHED)
        return false;

    wifiPmSecureZero(&g_auto, sizeof(g_auto));
    g_auto.port        = *port;
    g_auto.initialized = true;
    g_auto.stage       = WIFI_AUTO_IDLE;
    return true;
}

static bool acStart(void) {
    if (!g_auto.initialized || !wifiPwdMgr()->isReady() ||
        (g_auto.stage != WIFI_AUTO_IDLE && g_auto.stage != WIFI_AUTO_FINISHED))
        return false;

    acClearCurrent();
    memset(g_auto.tried, 0, sizeof(g_auto.tried));
    g_auto.tries   = 0u;
    g_auto.apCount = 0u;
    g_auto.stage   = WIFI_AUTO_SCANNING;
    if (!g_auto.port.startScan(g_auto.port.ctx)) {
        acFinish(WIFI_AUTO_SCAN_FAILED, WIFI_PM_OK);
        return false;
    }
    return true;
}

static void acOnScanComplete(bool success) {
    if (g_auto.stage != WIFI_AUTO_SCANNING)
        return;
    if (!success) {
        acFinish(WIFI_AUTO_SCAN_FAILED, WIFI_PM_OK);
        return;
    }
    g_auto.apCount = g_auto.port.scanCount(g_auto.port.ctx);
    if (g_auto.apCount > WIFI_PM_MAX_SCAN_APS) {
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK);
        return;
    }
    acAttemptNext();
}

static void acOnDisconnected(void) {
    if (g_auto.stage != WIFI_AUTO_WAIT_DISCONNECT)
        return;
    g_auto.stage = WIFI_AUTO_WAIT_CONNECT;

    if (!g_auto.port.requestConnect(g_auto.port.ctx, g_auto.current.apIndex,
                                    g_auto.current.password)) {
        /* Immediate HAL rejection is an unsuccessful attempt. */
        acAttemptNext();
    }
}

static void acOnConnectResult(bool success) {
    WifiPmAccessPoint ap;
    WifiPmResult      persisted;
    if (g_auto.stage != WIFI_AUTO_WAIT_CONNECT)
        return;

    if (!success || !g_auto.port.activateRoute(g_auto.port.ctx)) {
        acAttemptNext();
        return;
    }

    /* Metadata was snapshotted at selection time: a later scan can't swap
     * the BSSID/password record behind an in-flight attempt. */
    ap.ssid     = g_auto.current.ssid;
    ap.bssid    = g_auto.current.bssid;
    ap.security = g_auto.current.security;
    persisted   = wifiPwdMgr()->rememberSuccess(&ap, g_auto.current.password);
    acFinish(WIFI_AUTO_CONNECTED, persisted);
}

static void acOnTimeout(void) {
    if (g_auto.stage == WIFI_AUTO_SCANNING ||
        g_auto.stage == WIFI_AUTO_WAIT_DISCONNECT ||
        g_auto.stage == WIFI_AUTO_WAIT_CONNECT) {
        /* A timeout may produce a late HAL event. Terminate this scan; the
         * owner must abort/drain previous operations before starting again. */
        acFinish(WIFI_AUTO_TIMEOUT, WIFI_PM_OK);
    }
}

static void acCancel(void) {
    if (g_auto.stage != WIFI_AUTO_IDLE && g_auto.stage != WIFI_AUTO_FINISHED)
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK);
}

static WifiAutoStage acGetStage(void) { return g_auto.stage; }

OOP_CTOR(WifiAutoConnector) {
    if (self == NULL)
        return;
    self->init            = acInit;
    self->start           = acStart;
    self->onScanComplete  = acOnScanComplete;
    self->onDisconnected  = acOnDisconnected;
    self->onConnectResult = acOnConnectResult;
    self->onTimeout       = acOnTimeout;
    self->cancel          = acCancel;
    self->getStage        = acGetStage;
}

WifiAutoConnector* wifiAutoConnector(void) {
    CALL_ONCE(OOP_CALL_CTOR(WifiAutoConnector, &__wifiAutoConnector););
    return &__wifiAutoConnector;
}
