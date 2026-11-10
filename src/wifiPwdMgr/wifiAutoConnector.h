#ifndef WIFI_AUTO_CONNECTOR_H_
#define WIFI_AUTO_CONNECTOR_H_

#include <stdbool.h>
#include <stdint.h>

#include "oop.h"
#include "wifiPwdMgr/wifiPwdMgr.h"

/**
 * Automatic connection state.
 *
 * All methods and callbacks must run on the same serialized event-loop task.
 */
typedef enum {
    WIFI_AUTO_IDLE = 0,
    WIFI_AUTO_SCANNING,
    WIFI_AUTO_WAIT_DISCONNECT,
    WIFI_AUTO_WAIT_CONNECT,
    WIFI_AUTO_FINISHED
} WifiAutoStage;

/** Result passed to WifiAutoPort.finished(). */
typedef enum {
    WIFI_AUTO_CONNECTED = 0,
    WIFI_AUTO_NO_SAVED_AP,
    WIFI_AUTO_ALL_FAILED,
    WIFI_AUTO_SCAN_FAILED,
    WIFI_AUTO_HAL_ERROR,
    WIFI_AUTO_TIMEOUT,
    WIFI_AUTO_CANCELLED
} WifiAutoOutcome;

/**
 * Adapter from WifiAutoConnector to the existing Wi-Fi HAL and application.
 *
 * Returning true from a request callback means the request was accepted,
 * NOT that the operation has finished. The owner must later deliver the
 * matching completion event through onScanComplete(), onDisconnected(),
 * or onConnectResult(). Do not invoke those methods inline from a request
 * callback: queue the completion on the application's event loop instead.
 *
 * scanGet() returns references to stable scan-list entries. Do not modify
 * the scan list while an automatic-connection cycle is in progress.
 */
typedef struct {
    void* ctx;

    bool (*startScan)(void* ctx);
    uint16_t (*scanCount)(void* ctx);
    WifiPmScanGetFn scanGet;

    /* Must result in an onDisconnected() event, even if already disconnected.
     */
    bool (*requestDisconnect)(void* ctx);

    /* apIndex refers to the scan snapshot supplied by scanGet(). */
    bool (*requestConnect)(void* ctx, uint16_t apIndex, const char* password);

    /* Called only after a confirmed successful connection. */
    bool (*activateRoute)(void* ctx);

    /* Final outcome; persistence is the result of rememberSuccess(). */
    void (*finished)(void* ctx, WifiAutoOutcome outcome,
                     WifiPmResult persistence);
} WifiAutoPort;

/**
 * OOP singleton, matching WifiPwdMgr style: methods have no explicit self.
 *
 * Lifecycle:
 *   1. wifiPwdMgr()->init(...)
 *   2. wifiAutoConnector()->init(&port)
 *   3. wifiAutoConnector()->start()
 *   4. Forward confirmed HAL events to on*() methods.
 *
 * Timeout and cancellation only end this state machine; the application must
 * abort/drain any still-running HAL operation before calling start() again.
 */
OOP_CLASS(WifiAutoConnector) {
    OOP_METHOD(bool, init, const WifiAutoPort* port);
    OOP_METHOD(bool, isReady, void);
    OOP_METHOD(bool, start, void);

    OOP_METHOD(void, onScanComplete, bool success);
    OOP_METHOD(void, onDisconnected, void);
    OOP_METHOD(void, onConnectResult, bool success);
    OOP_METHOD(void, onTimeout, void);
    OOP_METHOD(void, cancel, void);

    OOP_METHOD(WifiAutoStage, getStage, void);
};

OOP_CTOR(WifiAutoConnector);
WifiAutoConnector* wifiAutoConnector(void);

#endif /* WIFI_AUTO_CONNECTOR_H_ */
