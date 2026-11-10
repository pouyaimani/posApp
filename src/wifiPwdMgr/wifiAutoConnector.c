#include "wifiAutoConnector.h"

#include <string.h>

#include "logger.h"

/*
 * Runtime state is private to this singleton, just like WifiPwdMgr.
 * No dynamic allocation and no explicit self argument in public methods.
 */
typedef struct {
    WifiAutoPort    port;
    uint8_t         tried[(WIFI_PM_MAX_SCAN_APS + 7u) / 8u];
    WifiPmCandidate current;
    uint16_t        apCount;
    uint16_t        tries;
    WifiAutoStage   stage;
    bool            initialized;
    bool            stopRequested;
    bool            safeToHandoff;
} WifiAutoRuntime;

static WifiAutoRuntime   g_auto;
static WifiAutoConnector __wifiAutoConnector;

static bool acIsActive(void) {
    return g_auto.stage == WIFI_AUTO_SCANNING ||
           g_auto.stage == WIFI_AUTO_WAIT_DISCONNECT ||
           g_auto.stage == WIFI_AUTO_WAIT_CONNECT;
}

static void acClearCurrent(void) {
    wifiPmSecureZero(&g_auto.current, sizeof(g_auto.current));
}

static void acFinish(WifiAutoOutcome outcome, WifiPmResult persistence,
                     bool halCompleted) {
    if (!acIsActive())
        return;

    /* Set terminal state before invoking application code. */
    g_auto.stage         = WIFI_AUTO_FINISHED;
    g_auto.stopRequested = false;
    g_auto.safeToHandoff = halCompleted;
    acClearCurrent();

    LOG_TRACE("WiFi auto: finished outcome=%d, persistence=%d, "
              "attempts=%u, handoffSafe=%d",
              (int)outcome, (int)persistence, (unsigned)g_auto.tries,
              (int)g_auto.safeToHandoff);

    if (g_auto.port.finished != NULL)
        g_auto.port.finished(g_auto.port.ctx, outcome, persistence);
}

static bool acWasTried(uint16_t index) {
    return (g_auto.tried[index / 8u] & (uint8_t)(1u << (index % 8u))) != 0u;
}

static void acMarkTried(uint16_t index) {
    g_auto.tried[index / 8u] |= (uint8_t)(1u << (index % 8u));
}

static void acAttemptNext(void) {
    WifiPwdMgr* pm = wifiPwdMgr();
    uint16_t    index;

    /* Stop before selecting a new AP after the previous request completed. */
    if (g_auto.stopRequested) {
        LOG_TRACE("WiFi auto: yielding to manual Wi-Fi");
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK, true);
        return;
    }

    acClearCurrent();

    if (!pm->pickCandidate(g_auto.port.scanGet, g_auto.port.ctx, g_auto.apCount,
                           g_auto.tried, sizeof(g_auto.tried),
                           &g_auto.current)) {
        acFinish(g_auto.tries == 0u ? WIFI_AUTO_NO_SAVED_AP
                                    : WIFI_AUTO_ALL_FAILED,
                 WIFI_PM_OK, true);
        return;
    }

    index = g_auto.current.apIndex;
    if (index >= g_auto.apCount || acWasTried(index)) {
        LOG_ERROR("WiFi auto: invalid candidate index=%u", (unsigned)index);
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK, true);
        return;
    }

    /* Mark before calling the HAL so the same scanned AP is not retried. */
    acMarkTried(index);
    ++g_auto.tries;
    g_auto.stage = WIFI_AUTO_WAIT_DISCONNECT;

    LOG_TRACE("WiFi auto: selected AP index=%u, attempt=%u", (unsigned)index,
              (unsigned)g_auto.tries);

    /* A separate, confirmed disconnect event must call onDisconnected(). */
    if (!g_auto.port.requestDisconnect(g_auto.port.ctx)) {
        LOG_ERROR("WiFi auto: disconnect request rejected");
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK, true);
    }
}

static bool acInit(const WifiAutoPort* port) {
    if (port == NULL || !wifiPwdMgr()->isReady() || port->startScan == NULL ||
        port->scanCount == NULL || port->scanGet == NULL ||
        port->requestDisconnect == NULL || port->requestConnect == NULL ||
        port->activateRoute == NULL)
        return false;

    /* Never replace port callbacks during an outstanding operation. */
    if (g_auto.initialized && (acIsActive() || !g_auto.safeToHandoff))
        return false;

    wifiPmSecureZero(&g_auto, sizeof(g_auto));
    g_auto.port = *port; /* Copy callbacks: caller may use a stack variable. */
    g_auto.initialized   = true;
    g_auto.stage         = WIFI_AUTO_IDLE;
    g_auto.safeToHandoff = true;

    LOG_TRACE("WiFi auto: connector initialized");
    return true;
}

static bool acIsReady(void) { return g_auto.initialized; }

static bool acStart(void) {
    if (!g_auto.initialized || !wifiPwdMgr()->isReady() || acIsActive() ||
        !g_auto.safeToHandoff)
        return false;

    acClearCurrent();
    memset(g_auto.tried, 0, sizeof(g_auto.tried));
    g_auto.apCount       = 0u;
    g_auto.tries         = 0u;
    g_auto.stage         = WIFI_AUTO_SCANNING;
    g_auto.stopRequested = false;
    g_auto.safeToHandoff = false;

    LOG_TRACE("WiFi auto: starting scan");
    if (!g_auto.port.startScan(g_auto.port.ctx)) {
        LOG_ERROR("WiFi auto: scan request rejected");
        acFinish(WIFI_AUTO_SCAN_FAILED, WIFI_PM_OK, true);
        return false;
    }
    return true;
}

static void acOnScanComplete(bool success) {
    if (g_auto.stage != WIFI_AUTO_SCANNING)
        return;

    if (g_auto.stopRequested) {
        LOG_TRACE("WiFi auto: scan completed, manual Wi-Fi takes priority");
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK, true);
        return;
    }

    if (!success) {
        acFinish(WIFI_AUTO_SCAN_FAILED, WIFI_PM_OK, true);
        return;
    }

    g_auto.apCount = g_auto.port.scanCount(g_auto.port.ctx);
    LOG_TRACE("WiFi auto: scan completed, AP count=%u",
              (unsigned)g_auto.apCount);

    if (g_auto.apCount > WIFI_PM_MAX_SCAN_APS) {
        LOG_ERROR("WiFi auto: scan AP count exceeds supported maximum");
        acFinish(WIFI_AUTO_HAL_ERROR, WIFI_PM_OK, true);
        return;
    }

    acAttemptNext();
}

static void acOnDisconnected(void) {
    if (g_auto.stage != WIFI_AUTO_WAIT_DISCONNECT)
        return;

    if (g_auto.stopRequested) {
        LOG_TRACE("WiFi auto: disconnect completed, yielding to manual Wi-Fi");
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK, true);
        return;
    }

    g_auto.stage = WIFI_AUTO_WAIT_CONNECT;
    LOG_TRACE("WiFi auto: disconnected; connecting AP index=%u",
              (unsigned)g_auto.current.apIndex);

    if (!g_auto.port.requestConnect(g_auto.port.ctx, g_auto.current.apIndex,
                                    g_auto.current.password)) {
        /* Immediate rejection is a failed attempt; try another scanned AP. */
        LOG_ERROR("WiFi auto: connect request rejected");
        acAttemptNext();
    }
}

static void acOnConnectResult(bool success) {
    WifiPmAccessPoint ap;
    WifiPmResult      persistence;

    if (g_auto.stage != WIFI_AUTO_WAIT_CONNECT)
        return;

    if (g_auto.stopRequested) {
        LOG_TRACE("WiFi auto: connect completed, yielding to manual Wi-Fi");
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK, true);
        return;
    }

    if (!success) {
        LOG_TRACE("WiFi auto: connect failed; trying next AP");
        acAttemptNext();
        return;
    }

    if (!g_auto.port.activateRoute(g_auto.port.ctx)) {
        LOG_ERROR("WiFi auto: route activation/check failed");
        acAttemptNext();
        return;
    }

    /* Use metadata snapshotted when the candidate was selected. */
    ap.ssid     = g_auto.current.ssid;
    ap.bssid    = g_auto.current.bssid;
    ap.security = g_auto.current.security;

    persistence = wifiPwdMgr()->rememberSuccess(&ap, g_auto.current.password);
    acFinish(WIFI_AUTO_CONNECTED, persistence, true);
}

static void acOnTimeout(void) {
    if (acIsActive()) {
        /* The owner must abort/drain a late HAL result before a new start. */
        LOG_ERROR("WiFi auto: operation timed out at stage=%d",
                  (int)g_auto.stage);
        acFinish(WIFI_AUTO_TIMEOUT, WIFI_PM_OK, false);
    }
}

static void acCancel(void) {
    if (acIsActive()) {
        /* Cancellation is logical; HAL abort/drain belongs to the port owner.
         */
        LOG_TRACE("WiFi auto: cancelled at stage=%d", (int)g_auto.stage);
        acFinish(WIFI_AUTO_CANCELLED, WIFI_PM_OK, false);
    }
}

static void acRequestStop(void) {
    if (!g_auto.initialized || !acIsActive())
        return;

    if (!g_auto.stopRequested) {
        LOG_TRACE("WiFi auto: manual stop requested at stage=%d; "
                  "waiting for HAL completion",
                  (int)g_auto.stage);
        g_auto.stopRequested = true;
    }
}

static bool acCanHandoff(void) {
    return g_auto.initialized && !acIsActive() && g_auto.safeToHandoff;
}

static WifiAutoStage acGetStage(void) { return g_auto.stage; }

OOP_CTOR(WifiAutoConnector) {
    if (self == NULL)
        return;

    self->init            = acInit;
    self->isReady         = acIsReady;
    self->start           = acStart;
    self->onScanComplete  = acOnScanComplete;
    self->onDisconnected  = acOnDisconnected;
    self->onConnectResult = acOnConnectResult;
    self->onTimeout       = acOnTimeout;
    self->requestStop     = acRequestStop;
    self->canHandoff      = acCanHandoff;
    self->cancel          = acCancel;
    self->getStage        = acGetStage;
}

WifiAutoConnector* wifiAutoConnector(void) {
    CALL_ONCE(OOP_CALL_CTOR(WifiAutoConnector, &__wifiAutoConnector););
    return &__wifiAutoConnector;
}
