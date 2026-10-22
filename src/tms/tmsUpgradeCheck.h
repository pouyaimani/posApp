#ifndef TMS_UPGRADE_CHECK_H
#define TMS_UPGRADE_CHECK_H

#include "tmsApi.h"

typedef void (*TmsUpgCheckCb)(const TmsCheckResult* r, void* userData);

typedef struct {
    TmsUpgCheckCb callback;
    void*         userData;
} TmsUpgCheckUserData;

TmsApiResult tmsUpgradeCheckStart(const char* host, uint16_t port,
                                  const char* serial, const char* brand,
                                  const char* current_version, TmsUpgCheckCb cb,
                                  void* userData);
TmsApiResult tmsUpgradeCheckCancel(void);

#endif
