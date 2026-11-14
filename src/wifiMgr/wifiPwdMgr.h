#ifndef WIFI_PWD_MGR_H_
#define WIFI_PWD_MGR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oop.h"
#include "dcfg.h"

/* Maximum stored networks. The storage.c backend has a 1024-byte file cap. */
#ifndef WIFI_PM_STORAGE_SLOTS
#define WIFI_PM_STORAGE_SLOTS 4u
#endif

/* Upper bound of the HAL's Wi-Fi scan results. */
#ifndef WIFI_PM_MAX_SCAN_APS
#define WIFI_PM_MAX_SCAN_APS 64u
#endif

/* Dedicated file and stable TLV ID; change the path to your device layout. */
#ifndef WIFI_PM_STORAGE_PATH
#define WIFI_PM_STORAGE_PATH USER_DATA_ROOT_DIR "mtwifi_pwd.dat"
#endif
#ifndef WIFI_PM_STORAGE_ID
#define WIFI_PM_STORAGE_ID 0x5A01u
#endif

#define WIFI_PM_SSID_CAPACITY     33u
#define WIFI_PM_PASSWORD_CAPACITY 65u
#define WIFI_PM_BSSID_CAPACITY    18u

#if (WIFI_PM_STORAGE_SLOTS < 1) || (WIFI_PM_STORAGE_SLOTS > 8)
#error "WIFI_PM_STORAGE_SLOTS must be 1..8 for the 1024-byte storage backend"
#endif
#if (WIFI_PM_MAX_SCAN_APS < 1) || (WIFI_PM_MAX_SCAN_APS > 65535)
#error "WIFI_PM_MAX_SCAN_APS must be 1..65535"
#endif

/* Limitations: SSID is treated as a NUL-terminated string (not arbitrary
 * bytes). */
typedef struct {
    const char* ssid;     /* Unnormalized SSID from Wi-Fi HAL */
    const char* bssid;    /* MAC as XX:XX:XX:XX:XX:XX; optional */
    uint32_t    security; /* Same numeric security mode as Wi-Fi HAL */
} WifiPmAccessPoint;

typedef enum {
    WIFI_PM_OK = 0,
    WIFI_PM_ERR_ARGUMENT,
    WIFI_PM_ERR_NOT_READY,
    WIFI_PM_ERR_IO,
    WIFI_PM_ERR_CORRUPT,
    WIFI_PM_ERR_NOT_FOUND
} WifiPmResult;

typedef bool (*WifiPmScanGetFn)(void* ctx, uint16_t index,
                                WifiPmAccessPoint* out);

typedef struct {
    uint16_t apIndex;
    char     ssid[WIFI_PM_SSID_CAPACITY];
    char     bssid[WIFI_PM_BSSID_CAPACITY];
    char     password[WIFI_PM_PASSWORD_CAPACITY];
    uint32_t security;
} WifiPmCandidate;

/* Like Nth: methods have no explicit self, implementation is singleton. */
OOP_CLASS(WifiPwdMgr) {
    OOP_METHOD(WifiPmResult, init, uint8_t activeLimit);
    OOP_METHOD(WifiPmResult, rememberSuccess, const WifiPmAccessPoint* ap,
               const char* password);
    OOP_METHOD(WifiPmResult, lookup, const WifiPmAccessPoint* ap,
               char* password, size_t capacity);
    OOP_METHOD(WifiPmResult, forget, const char* ssid, uint32_t security);
    OOP_METHOD(WifiPmResult, clear, void);
    OOP_METHOD(WifiPmResult, flush, void);
    OOP_METHOD(WifiPmResult, setLimit, uint8_t activeLimit);
    OOP_METHOD(uint8_t, count, void);
    OOP_METHOD(bool, isReady, void);
    OOP_METHOD(bool, isDirty, void);
    /* Tried bitmap needs ceil(apCount/8) bytes. */
    OOP_METHOD(bool, pickCandidate, WifiPmScanGetFn getAp, void* scanCtx,
               uint16_t apCount, const uint8_t* triedBits, size_t triedBytes,
               WifiPmCandidate* candidate);
};

OOP_CTOR(WifiPwdMgr);
WifiPwdMgr* wifiPwdMgr(void);
void        wifiPmSecureZero(void* buffer, size_t bytes);

#endif
