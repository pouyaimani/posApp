#include "tmsUpgrade.h"

#include <stdlib.h>
#include <string.h>

static void finish(TmsUpgrade *upgrade, TmsUpgradeState state,
                   TmsUpgradeResult result)
{
    upgrade->state = state;
    upgrade->result = result;
    if (upgrade->onDone != NULL)
        upgrade->onDone(upgrade, result, upgrade->userData);
}

static void checkDone(TmsApiClient *api, bool success, int serverCode,
                      void *userData)
{
    TmsUpgrade *upgrade = (TmsUpgrade *)userData;
    (void)api;
    if (upgrade == NULL || upgrade->state != TMS_UPGRADE_CHECKING)
        return;
    if (!success) {
        finish(upgrade, TMS_UPGRADE_FAILED,
               serverCode < 0 ? TMS_UPGRADE_RESULT_HTTP
                              : TMS_UPGRADE_RESULT_SERVER);
        return;
    }
    if (!upgrade->info.isHasNewVersion) {
        finish(upgrade, TMS_UPGRADE_NO_UPDATE,
               TMS_UPGRADE_RESULT_NO_UPDATE);
        return;
    }
    upgrade->state = TMS_UPGRADE_DOWNLOADING;
    upgrade->pendingProcess = true;
}

static void downloadDone(TmsDownloader *downloader, bool success,
                         void *userData)
{
    TmsUpgrade *upgrade = (TmsUpgrade *)userData;
    (void)downloader;
    if (upgrade == NULL || upgrade->state != TMS_UPGRADE_DOWNLOADING)
        return;
    if (!success) {
        finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_DOWNLOAD);
        return;
    }
    upgrade->state = TMS_UPGRADE_VERIFYING;
    upgrade->pendingProcess = true;
}

void tmsUpgradeInit(TmsUpgrade *upgrade, const char *host, uint16_t port,
                    const TmsPlatformOps *platform,
                    TmsUpgradeDoneCallback callback, void *userData)
{
    if (upgrade == NULL)
        return;
    memset(upgrade, 0, sizeof(*upgrade));
    upgrade->platform = platform;
    upgrade->onDone = callback;
    upgrade->userData = userData;
    upgrade->state = TMS_UPGRADE_IDLE;
    tmsApiClientInit(&upgrade->api, host, port);
    tmsDownloaderInit(&upgrade->downloader, platform);
}

TmsUpgradeResult tmsUpgradeStart(TmsUpgrade *upgrade, const char *deviceSn,
                                 const char *brandCode,
                                 const char *currentVersion,
                                 int packResourceType)
{
    HttpFlowResult result;
    if (upgrade == NULL || upgrade->platform == NULL || deviceSn == NULL ||
        brandCode == NULL || currentVersion == NULL)
        return TMS_UPGRADE_RESULT_ARGUMENT;
    if (upgrade->state != TMS_UPGRADE_IDLE && tmsUpgradeIsFinished(upgrade))
        upgrade->state = TMS_UPGRADE_IDLE;
    if (upgrade->state != TMS_UPGRADE_IDLE)
        return TMS_UPGRADE_RESULT_BUSY;
    upgrade->state = TMS_UPGRADE_CHECKING;
    result = tmsApiCheckUpgradeStart(
        &upgrade->api, deviceSn, brandCode, currentVersion, packResourceType,
        &upgrade->info, checkDone, upgrade);
    if (result != HTTP_FLOW_OK) {
        finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_HTTP);
        return TMS_UPGRADE_RESULT_HTTP;
    }
    return TMS_UPGRADE_RESULT_OK;
}

void tmsUpgradeProcess(TmsUpgrade *upgrade)
{
    uint32_t crc;
    if (upgrade == NULL)
        return;
    if (upgrade->state == TMS_UPGRADE_DOWNLOADING) {
        tmsDownloaderProcess(&upgrade->downloader);
        if (!upgrade->pendingProcess)
            return;
    } else if (!upgrade->pendingProcess) {
        return;
    }
    upgrade->pendingProcess = false;
    switch (upgrade->state) {
    case TMS_UPGRADE_DOWNLOADING:
        if (tmsDownloaderStart(&upgrade->downloader, upgrade->info.url,
                               TMS_APUPDATE_PATH,
                               (uint32_t)upgrade->info.fileSize,
                               TMS_APUPDATE_HEADER_LEN, downloadDone,
                               upgrade) != HTTP_FLOW_OK)
            finish(upgrade, TMS_UPGRADE_FAILED,
                   TMS_UPGRADE_RESULT_DOWNLOAD);
        break;
    case TMS_UPGRADE_VERIFYING:
        if (!tmsInstallerVerify(upgrade->platform, TMS_APUPDATE_PATH,
                                TMS_APUPDATE_HEADER_LEN,
                                (uint32_t)upgrade->info.fileSize,
                                upgrade->info.hash)) {
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_VERIFY);
            break;
        }
        upgrade->state = TMS_UPGRADE_PREPARING;
        upgrade->pendingProcess = true;
        break;
    case TMS_UPGRADE_PREPARING:
        crc = (uint32_t)strtoul(upgrade->info.hash, NULL, 16);
        if (!tmsInstallerPrepare(upgrade->platform, TMS_APUPDATE_PATH,
                                 "apapp.bin",
                                 (uint32_t)upgrade->info.fileSize, crc)) {
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_PREPARE);
            break;
        }
        upgrade->state = TMS_UPGRADE_ACTIVATING;
        upgrade->pendingProcess = true;
        break;
    case TMS_UPGRADE_ACTIVATING:
        if (!tmsInstallerActivate(upgrade->platform)) {
            finish(upgrade, TMS_UPGRADE_FAILED,
                   TMS_UPGRADE_RESULT_ACTIVATE);
            break;
        }
        finish(upgrade, TMS_UPGRADE_COMPLETED, TMS_UPGRADE_RESULT_OK);
        if (upgrade->platform->reboot != NULL)
            upgrade->platform->reboot();
        break;
    default:
        break;
    }
}

void tmsUpgradeCancel(TmsUpgrade *upgrade)
{
    if (upgrade == NULL || tmsUpgradeIsFinished(upgrade))
        return;
    if (upgrade->state == TMS_UPGRADE_CHECKING)
        tmsApiClientCancel(&upgrade->api);
    if (upgrade->state == TMS_UPGRADE_DOWNLOADING)
        tmsDownloaderCancel(&upgrade->downloader);
    finish(upgrade, TMS_UPGRADE_CANCELLED, TMS_UPGRADE_RESULT_CANCELLED);
}

uint32_t tmsUpgradeProgress(const TmsUpgrade *upgrade)
{
    if (upgrade == NULL)
        return 0u;
    if (upgrade->state == TMS_UPGRADE_DOWNLOADING)
        return tmsDownloaderProgress(&upgrade->downloader);
    return upgrade->state == TMS_UPGRADE_COMPLETED ? 100u : 0u;
}

bool tmsUpgradeIsFinished(const TmsUpgrade *upgrade)
{
    return upgrade != NULL &&
           (upgrade->state == TMS_UPGRADE_COMPLETED ||
            upgrade->state == TMS_UPGRADE_NO_UPDATE ||
            upgrade->state == TMS_UPGRADE_CANCELLED ||
            upgrade->state == TMS_UPGRADE_FAILED);
}
