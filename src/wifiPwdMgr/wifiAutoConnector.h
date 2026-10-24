#ifndef WIFI_AUTO_CONNECTOR_H_
#define WIFI_AUTO_CONNECTOR_H_

#include "wifiPwdMgr.h"

typedef enum {
    WIFI_AUTO_IDLE = 0,
    WIFI_AUTO_SCANNING,
    WIFI_AUTO_WAIT_DISCONNECT,
    WIFI_AUTO_WAIT_CONNECT,
    WIFI_AUTO_FINISHED
} WifiAutoStage;

typedef enum {
    WIFI_AUTO_CONNECTED = 0,
    WIFI_AUTO_NO_SAVED_AP,
    WIFI_AUTO_ALL_FAILED,
    WIFI_AUTO_SCAN_FAILED,
    WIFI_AUTO_HAL_ERROR,
    WIFI_AUTO_TIMEOUT,
    WIFI_AUTO_CANCELLED
} WifiAutoOutcome;

typedef struct {
    void* ctx;
    bool (*startScan)(void* ctx);
    uint16_t (*scanCount)(void* ctx);
    WifiPmScanGetFn scanGet;
    bool (*requestDisconnect)(void* ctx);
    bool (*requestConnect)(void* ctx, uint16_t apIndex, const char* password);
    bool (*activateRoute)(void* ctx);
    void (*finished)(void* ctx, WifiAutoOutcome outcome,
                     WifiPmResult persistence);
} WifiAutoPort;

/* Event-driven singleton. HAL requests must be asynchronous; dispatch only
 * confirmed results to on*() methods, on the same serialized task.
 */
OOP_CLASS(WifiAutoConnector) {
    OOP_METHOD(bool, init, const WifiAutoPort* port);
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

#endif
