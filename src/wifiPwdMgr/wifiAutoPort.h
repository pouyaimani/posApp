#ifndef WIFI_AUTO_PORT_H_
#define WIFI_AUTO_PORT_H_

#include "wifiPwdMgr.h"

bool wifiAutoPortStartScan(void* ctx);

uint16_t wifiAutoPortScanCount(void* ctx);

bool wifiAutoPortScanGet(void* ctx, uint16_t index, WifiPmAccessPoint* out);

bool wifiAutoPortRequestDisconnect(void* ctx);

bool wifiAutoPortRequestConnect(void* ctx, uint16_t apIndex,
                                const char* password);

bool wifiAutoPortActivateRoute(void* ctx);

void wifiAutoPortFinished(void* ctx, WifiAutoOutcome outcome,
                          WifiPmResult persistence);

bool wifiAutoPortFill(WifiAutoPort* port);

#endif