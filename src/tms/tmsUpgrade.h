#ifndef TMS_UPGRADE_H
#define TMS_UPGRADE_H

#include "tmsDownloader.h"
#include "tmsInstaller.h"
#include "tmsUpgradeInfo.h"
#include "tmsApiClient.h"

typedef enum {
    TMS_UPGRADE_IDLE = 0,
    TMS_UPGRADE_CHECKING,
    TMS_UPGRADE_DOWNLOADING,
    TMS_UPGRADE_VERIFYING,
    TMS_UPGRADE_PREPARING,
    TMS_UPGRADE_ACTIVATING,
    TMS_UPGRADE_COMPLETED,
    TMS_UPGRADE_NO_UPDATE,
    TMS_UPGRADE_CANCELLED,
    TMS_UPGRADE_FAILED
} TmsUpgradeState;

typedef enum {
    TMS_UPGRADE_RESULT_OK = 0,
    TMS_UPGRADE_RESULT_NO_UPDATE,
    TMS_UPGRADE_RESULT_CANCELLED,
    TMS_UPGRADE_RESULT_HTTP,
    TMS_UPGRADE_RESULT_SERVER,
    TMS_UPGRADE_RESULT_DOWNLOAD,
    TMS_UPGRADE_RESULT_VERIFY,
    TMS_UPGRADE_RESULT_PREPARE,
    TMS_UPGRADE_RESULT_ACTIVATE,
    TMS_UPGRADE_RESULT_BUSY,
    TMS_UPGRADE_RESULT_ARGUMENT
} TmsUpgradeResult;

typedef struct TmsUpgrade TmsUpgrade;
typedef void (*TmsUpgradeDoneCallback)(TmsUpgrade*      upgrade,
                                       TmsUpgradeResult result, void* userData);

struct TmsUpgrade {
    TmsApiClient           api;
    TmsDownloader          downloader;
    const TmsPlatformOps*  platform;
    UpgradeInfo            info;
    TmsUpgradeState        state;
    TmsUpgradeResult       result;
    bool                   pendingProcess;
    TmsUpgradeDoneCallback onDone;
    void*                  userData;
};

void tmsUpgradeInit(TmsUpgrade* upgrade, const char* host, uint16_t port,
                    const TmsPlatformOps*  platform,
                    TmsUpgradeDoneCallback callback, void* userData);
TmsUpgradeResult tmsUpgradeStart(TmsUpgrade* upgrade, const char* deviceSn,
                                 const char* brandCode,
                                 const char* currentVersion,
                                 int         packResourceType);
void             tmsUpgradeProcess(TmsUpgrade* upgrade);
void             tmsUpgradeCancel(TmsUpgrade* upgrade);
uint32_t         tmsUpgradeProgress(const TmsUpgrade* upgrade);
bool             tmsUpgradeIsFinished(const TmsUpgrade* upgrade);

#endif
