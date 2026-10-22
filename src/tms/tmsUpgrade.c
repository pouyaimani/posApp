#include "tmsUpgrade.h"
#include "error.h"
#include "logger.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * TMS upgrade coordinator.
 *
 * LOG_TRACE records meaningful lifecycle events and transitions. Frequent
 * polling of tmsUpgradeProcess(), tmsUpgradeProgress() and
 * tmsUpgradeIsFinished() is intentionally not logged.
 *
 * Device identifiers, full URLs and cryptographic values are not logged.
 */

static const char* stateName(TmsUpgradeState state) {
    switch (state) {
    case TMS_UPGRADE_IDLE:
        return "IDLE";
    case TMS_UPGRADE_CHECKING:
        return "CHECKING";
    case TMS_UPGRADE_DOWNLOADING:
        return "DOWNLOADING";
    case TMS_UPGRADE_VERIFYING:
        return "VERIFYING";
    case TMS_UPGRADE_PREPARING:
        return "PREPARING";
    case TMS_UPGRADE_ACTIVATING:
        return "ACTIVATING";
    case TMS_UPGRADE_COMPLETED:
        return "COMPLETED";
    case TMS_UPGRADE_NO_UPDATE:
        return "NO_UPDATE";
    case TMS_UPGRADE_CANCELLED:
        return "CANCELLED";
    case TMS_UPGRADE_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

static const char* resultName(TmsUpgradeResult result) {
    switch (result) {
    case TMS_UPGRADE_RESULT_OK:
        return "OK";
    case TMS_UPGRADE_RESULT_ARGUMENT:
        return "ARGUMENT";
    case TMS_UPGRADE_RESULT_BUSY:
        return "BUSY";
    case TMS_UPGRADE_RESULT_HTTP:
        return "HTTP";
    case TMS_UPGRADE_RESULT_SERVER:
        return "SERVER";
    case TMS_UPGRADE_RESULT_NO_UPDATE:
        return "NO_UPDATE";
    case TMS_UPGRADE_RESULT_DOWNLOAD:
        return "DOWNLOAD";
    case TMS_UPGRADE_RESULT_VERIFY:
        return "VERIFY";
    case TMS_UPGRADE_RESULT_PREPARE:
        return "PREPARE";
    case TMS_UPGRADE_RESULT_ACTIVATE:
        return "ACTIVATE";
    case TMS_UPGRADE_RESULT_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}

/** Log a real state transition, not every process-loop iteration. */
static void setState(TmsUpgrade* upgrade, TmsUpgradeState newState) {
    if (upgrade->state != newState) {
        LOG_TRACE("TMS UPG state: %s(%d) -> %s(%d)", stateName(upgrade->state),
                  (int)upgrade->state, stateName(newState), (int)newState);
    }
    upgrade->state = newState;
}

/**
 * Parse a hexadecimal CRC-32 without silently accepting trailing junk,
 * overflow, or an empty string. The optional 0x prefix is supported.
 */
static bool parseHexCrc32(const char* text, uint32_t* outCrc) {
    uint32_t value  = 0u;
    size_t   digits = 0u;

    if (text == NULL || outCrc == NULL || text[0] == '\0') {
        LOG_ERROR("TMS UPG parseHexCrc32(): missing hash or output");
        return false;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        text += 2;

    while (*text != '\0') {
        uint32_t            digit;
        const unsigned char ch = (unsigned char)*text;

        if (ch >= '0' && ch <= '9')
            digit = (uint32_t)(ch - '0');
        else if (ch >= 'a' && ch <= 'f')
            digit = (uint32_t)(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F')
            digit = (uint32_t)(ch - 'A' + 10);
        else {
            LOG_ERROR(
                "TMS UPG parseHexCrc32(): invalid hex character at offset=%lu",
                (unsigned long)digits);
            return false;
        }

        if (digits >= 8u) {
            LOG_ERROR("TMS UPG parseHexCrc32(): CRC exceeds 32 bits");
            return false;
        }

        value = (value << 4) | digit;
        digits++;
        text++;
    }

    if (digits == 0u) {
        LOG_ERROR("TMS UPG parseHexCrc32(): empty CRC value");
        return false;
    }

    *outCrc = value;
    LOG_TRACE("TMS UPG parseHexCrc32(): valid CRC-32 digits=%lu",
              (unsigned long)digits);
    return true;
}

/**
 * Finalize an upgrade exactly once for the active operation.
 *
 * Releasing before calling onDone avoids touching a possibly freed or
 * reinitialized upgrade after the user callback. The user callback may
 * release the upgrade object, so no upgrade access is allowed afterward.
 */
static void finish(TmsUpgrade* upgrade, TmsUpgradeState state,
                   TmsUpgradeResult result) {
    TmsUpgradeDoneCallback callback;
    void*                  userData;

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG finish(): upgrade is NULL");
        return;
    }

    LOG_TRACE("TMS UPG finish(): current=%s final=%s result=%s(%d)",
              stateName(upgrade->state), stateName(state), resultName(result),
              (int)result);

    setState(upgrade, state);
    upgrade->result         = result;
    upgrade->pendingProcess = false;

    callback = upgrade->onDone;
    userData = upgrade->userData;

    LOG_TRACE("TMS UPG finish(): releasing API client callbackSet=%d",
              (int)(callback != NULL));
    tmsApiClientRelease(&upgrade->api);

    if (callback != NULL) {
        LOG_TRACE("TMS UPG finish(): invoking completion callback result=%s",
                  resultName(result));
        callback(upgrade, result, userData);
        /* Do not access upgrade after callback. */
    } else {
        LOG_TRACE("TMS UPG finish(): no completion callback registered");
    }
}

/** Completion of the server upgrade-availability request. */
static void checkDone(TmsApiClient* api, bool success, int serverCode,
                      void* userData) {
    TmsUpgrade* upgrade = (TmsUpgrade*)userData;
    (void)api;

    LOG_TRACE("TMS UPG checkDone(): success=%d serverCode=%d", (int)success,
              serverCode);

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG checkDone(): upgrade context is NULL");
        return;
    }
    if (upgrade->state != TMS_UPGRADE_CHECKING) {
        LOG_TRACE("TMS UPG checkDone(): ignored in state=%s",
                  stateName(upgrade->state));
        return;
    }

    if (!success) {
        TmsUpgradeResult failure = serverCode < 0 ? TMS_UPGRADE_RESULT_HTTP
                                                  : TMS_UPGRADE_RESULT_SERVER;
        LOG_ERROR("TMS UPG checkDone(): request/parse failed code=%d result=%s",
                  serverCode, resultName(failure));
        finish(upgrade, TMS_UPGRADE_FAILED, failure);
        return;
    }

    /* JSON parsing success is not the same as server business success. */
    if (serverCode != 0) {
        LOG_ERROR("TMS UPG checkDone(): server rejected check code=%d",
                  serverCode);
        finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_SERVER);
        return;
    }

    LOG_TRACE("TMS UPG checkDone(): hasNewVersion=%d fileSize=%ld",
              (int)upgrade->info.isHasNewVersion, (long)upgrade->info.fileSize);

    if (!upgrade->info.isHasNewVersion) {
        LOG_TRACE("TMS UPG checkDone(): device is up to date");
        finish(upgrade, TMS_UPGRADE_NO_UPDATE, TMS_UPGRADE_RESULT_NO_UPDATE);
        return;
    }

    LOG_TRACE("TMS UPG checkDone(): update available, scheduling download");
    setState(upgrade, TMS_UPGRADE_DOWNLOADING);
    upgrade->pendingProcess = true;
}

/** Completion of all HTTP range downloads. */
static void downloadDone(TmsDownloader* downloader, bool success,
                         void* userData) {
    TmsUpgrade* upgrade = (TmsUpgrade*)userData;

    LOG_TRACE("TMS UPG downloadDone(): success=%d downloaderSet=%d",
              (int)success, (int)(downloader != NULL));

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG downloadDone(): upgrade context is NULL");
        return;
    }
    if (upgrade->state != TMS_UPGRADE_DOWNLOADING) {
        LOG_TRACE("TMS UPG downloadDone(): ignored in state=%s",
                  stateName(upgrade->state));
        return;
    }

    LOG_TRACE("TMS UPG downloadDone(): progress=%lu%% downloaded=%lu/%lu",
              (unsigned long)tmsDownloaderProgress(&upgrade->downloader),
              (unsigned long)upgrade->downloader.downloaded,
              (unsigned long)upgrade->downloader.totalSize);

    if (!success) {
        LOG_ERROR("TMS UPG downloadDone(): download failed");
        finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_DOWNLOAD);
        return;
    }

    LOG_TRACE(
        "TMS UPG downloadDone(): download completed, queuing verification");
    setState(upgrade, TMS_UPGRADE_VERIFYING);
    upgrade->pendingProcess = true;
}

/** Initialize the coordinator and its HTTP/download subclients. */
void tmsUpgradeInit(TmsUpgrade* upgrade, const char* host, uint16_t port,
                    const TmsPlatformOps*  platform,
                    TmsUpgradeDoneCallback callback, void* userData) {
    LOG_TRACE(
        "TMS UPG tmsUpgradeInit(): begin port=%u hostSet=%d platformSet=%d",
        (unsigned int)port, (int)(host != NULL), (int)(platform != NULL));

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG tmsUpgradeInit(): upgrade is NULL");
        return;
    }

    memset(upgrade, 0, sizeof(*upgrade));
    upgrade->platform = platform;
    upgrade->onDone   = callback;
    upgrade->userData = userData;
    upgrade->state    = TMS_UPGRADE_IDLE;

    if (platform == NULL || callback == NULL)
        LOG_ERROR("TMS UPG tmsUpgradeInit(): platform or callback missing");

    tmsApiClientInit(&upgrade->api, host, port);
    tmsDownloaderInit(&upgrade->downloader, platform);

    LOG_TRACE("TMS UPG tmsUpgradeInit(): initialized state=%s callbackSet=%d",
              stateName(upgrade->state), (int)(callback != NULL));
}

/** Start upgrade availability check. */
TmsUpgradeResult tmsUpgradeStart(TmsUpgrade* upgrade, const char* deviceSn,
                                 const char* brandCode,
                                 const char* currentVersion,
                                 int         packResourceType) {
    HttpFlowResult result;

    LOG_TRACE("TMS UPG tmsUpgradeStart(): requested resourceType=%d",
              packResourceType);

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG tmsUpgradeStart(): upgrade is NULL");
        return TMS_UPGRADE_RESULT_ARGUMENT;
    }

    if (upgrade->platform == NULL || deviceSn == NULL || brandCode == NULL ||
        currentVersion == NULL) {
        LOG_ERROR("TMS UPG tmsUpgradeStart(): invalid args platform=%d "
                  "serial=%d brand=%d version=%d",
                  (int)(upgrade->platform != NULL), (int)(deviceSn != NULL),
                  (int)(brandCode != NULL), (int)(currentVersion != NULL));
        return TMS_UPGRADE_RESULT_ARGUMENT;
    }

    LOG_TRACE("TMS UPG tmsUpgradeStart(): currentState=%s",
              stateName(upgrade->state));

    if (upgrade->state != TMS_UPGRADE_IDLE && tmsUpgradeIsFinished(upgrade)) {
        LOG_TRACE("TMS UPG tmsUpgradeStart(): resetting terminal state for new "
                  "check");
        setState(upgrade, TMS_UPGRADE_IDLE);
    }

    if (upgrade->state != TMS_UPGRADE_IDLE) {
        LOG_ERROR("TMS UPG tmsUpgradeStart(): already active state=%s",
                  stateName(upgrade->state));
        return TMS_UPGRADE_RESULT_BUSY;
    }

    upgrade->pendingProcess = false;
    setState(upgrade, TMS_UPGRADE_CHECKING);
    LOG_TRACE("TMS UPG tmsUpgradeStart(): sending upgrade check");

    result = tmsApiCheckUpgradeStart(&upgrade->api, deviceSn, brandCode,
                                     currentVersion, packResourceType,
                                     &upgrade->info, checkDone, upgrade);

    if (result != HTTP_FLOW_OK) {
        LOG_ERROR(
            "TMS UPG tmsUpgradeStart(): check HTTP startup failed result=%d",
            (int)result);
        finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_HTTP);
        return TMS_UPGRADE_RESULT_HTTP;
    }

    /* Do not read upgrade here: synchronous completion may have called onDone.
     */
    LOG_TRACE("TMS UPG tmsUpgradeStart(): upgrade check accepted");
    return TMS_UPGRADE_RESULT_OK;
}

/**
 * Advance scheduled upgrade stages.
 * Must be called regularly by the application while the upgrade is active.
 */
void tmsUpgradeProcess(TmsUpgrade* upgrade) {
    uint32_t crc;

    if (upgrade == NULL)
        return; /* Called frequently: avoid logging each null poll. */

    if (upgrade->state == TMS_UPGRADE_DOWNLOADING) {
        /* The downloader logs individual chunk requests and completions. */
        tmsDownloaderProcess(&upgrade->downloader);
        if (!upgrade->pendingProcess)
            return;
    } else if (!upgrade->pendingProcess) {
        return;
    }

    LOG_TRACE("TMS UPG tmsUpgradeProcess(): processing pending state=%s",
              stateName(upgrade->state));
    upgrade->pendingProcess = false;

    switch (upgrade->state) {
    case TMS_UPGRADE_DOWNLOADING: {
        HttpFlowResult requestResult;

        if (upgrade->info.fileSize <= 0 || upgrade->info.url[0] == '\0' ||
            upgrade->info.hash[0] == '\0') {
            LOG_ERROR("TMS UPG download: invalid metadata size=%ld urlSet=%d "
                      "hashSet=%d",
                      (long)upgrade->info.fileSize,
                      (int)(upgrade->info.url[0] != '\0'),
                      (int)(upgrade->info.hash[0] != '\0'));
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_DOWNLOAD);
            return;
        }

        LOG_TRACE(
            "TMS UPG download: starting size=%lu headerReserve=%lu path=%s",
            (unsigned long)(uint32_t)upgrade->info.fileSize,
            (unsigned long)TMS_APUPDATE_HEADER_LEN, TMS_APUPDATE_PATH);

        requestResult = tmsDownloaderStart(
            &upgrade->downloader, upgrade->info.url, TMS_APUPDATE_PATH,
            (uint32_t)upgrade->info.fileSize, TMS_APUPDATE_HEADER_LEN,
            downloadDone, upgrade);

        if (requestResult != HTTP_FLOW_OK) {
            LOG_ERROR("TMS UPG download: failed to start request result=%d",
                      (int)requestResult);
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_DOWNLOAD);
            return;
        }

        LOG_TRACE("TMS UPG download: first request accepted");
        break;
    }

    case TMS_UPGRADE_VERIFYING:
        LOG_TRACE("TMS UPG verify: beginning file=%s offset=%lu size=%lu",
                  TMS_APUPDATE_PATH, (unsigned long)TMS_APUPDATE_HEADER_LEN,
                  (unsigned long)(uint32_t)upgrade->info.fileSize);

        if (!tmsInstallerVerify(
                upgrade->platform, TMS_APUPDATE_PATH, TMS_APUPDATE_HEADER_LEN,
                (uint32_t)upgrade->info.fileSize, upgrade->info.hash)) {
            LOG_ERROR("TMS UPG verify: integrity verification failed");
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_VERIFY);
            return;
        }

        LOG_TRACE("TMS UPG verify: successful, scheduling prepare");
        setState(upgrade, TMS_UPGRADE_PREPARING);
        upgrade->pendingProcess = true;
        break;

    case TMS_UPGRADE_PREPARING:
        LOG_TRACE("TMS UPG prepare: validating hexadecimal CRC");

        if (!parseHexCrc32(upgrade->info.hash, &crc)) {
            LOG_ERROR("TMS UPG prepare: invalid CRC format");
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_PREPARE);
            return;
        }

        LOG_TRACE("TMS UPG prepare: preparing apapp.bin size=%lu crc=0x%08lX",
                  (unsigned long)(uint32_t)upgrade->info.fileSize,
                  (unsigned long)crc);

        if (!tmsInstallerPrepare(upgrade->platform, TMS_APUPDATE_PATH,
                                 "apapp.bin", (uint32_t)upgrade->info.fileSize,
                                 crc)) {
            LOG_ERROR("TMS UPG prepare: installer preparation failed");
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_PREPARE);
            return;
        }

        LOG_TRACE("TMS UPG prepare: successful, scheduling activation");
        setState(upgrade, TMS_UPGRADE_ACTIVATING);
        upgrade->pendingProcess = true;
        break;

    case TMS_UPGRADE_ACTIVATING: {
        /* Snapshot platform before finish(): onDone may free upgrade. */
        const TmsPlatformOps* platform = upgrade->platform;
        bool rebootAvailable = platform != NULL && platform->reboot != NULL;

        LOG_TRACE("TMS UPG activate: beginning rebootAvailable=%d",
                  (int)rebootAvailable);

        if (!tmsInstallerActivate(platform)) {
            LOG_ERROR("TMS UPG activate: installer activation failed");
            finish(upgrade, TMS_UPGRADE_FAILED, TMS_UPGRADE_RESULT_ACTIVATE);
            return;
        }

        LOG_TRACE("TMS UPG activate: successful; notifying completion");
        finish(upgrade, TMS_UPGRADE_COMPLETED, TMS_UPGRADE_RESULT_OK);

        /* Do not dereference upgrade here; callback may have freed it.
         * The platform operations object must outlive the callback. */
        if (rebootAvailable) {
            LOG_TRACE("TMS UPG activate: invoking platform reboot");
            platform->reboot();
        } else {
            LOG_TRACE("TMS UPG activate: reboot callback unavailable");
        }
        return;
    }

    default:
        LOG_ERROR(
            "TMS UPG tmsUpgradeProcess(): unexpected pending state=%s(%d)",
            stateName(upgrade->state), (int)upgrade->state);
        break;
    }
}

/** Cancel an active update, without reporting cancellation as an HTTP error. */
void tmsUpgradeCancel(TmsUpgrade* upgrade) {
    TmsUpgradeState previousState;

    LOG_TRACE("TMS UPG tmsUpgradeCancel(): requested");

    if (upgrade == NULL) {
        LOG_ERROR("TMS UPG tmsUpgradeCancel(): upgrade is NULL");
        return;
    }

    previousState = upgrade->state;
    LOG_TRACE("TMS UPG tmsUpgradeCancel(): currentState=%s pending=%d",
              stateName(previousState), (int)upgrade->pendingProcess);

    if (previousState == TMS_UPGRADE_IDLE || tmsUpgradeIsFinished(upgrade)) {
        LOG_TRACE("TMS UPG tmsUpgradeCancel(): ignored (no active upgrade)");
        return;
    }

    /* Change state first so reentrant callbacks ignore cancellation. */
    setState(upgrade, TMS_UPGRADE_CANCELLED);
    upgrade->pendingProcess = false;

    if (previousState == TMS_UPGRADE_CHECKING) {
        LOG_TRACE("TMS UPG tmsUpgradeCancel(): cancelling API check");
        tmsApiClientCancel(&upgrade->api);
    } else if (previousState == TMS_UPGRADE_DOWNLOADING) {
        LOG_TRACE("TMS UPG tmsUpgradeCancel(): cancelling downloader");
        tmsDownloaderCancel(&upgrade->downloader);
    } else {
        LOG_TRACE("TMS UPG tmsUpgradeCancel(): stopped queued stage=%s",
                  stateName(previousState));
    }

    finish(upgrade, TMS_UPGRADE_CANCELLED, TMS_UPGRADE_RESULT_CANCELLED);
}

/** Return integer download percentage without flooding polling-loop logs. */
uint32_t tmsUpgradeProgress(const TmsUpgrade* upgrade) {
    if (upgrade == NULL)
        return 0u;
    if (upgrade->state == TMS_UPGRADE_DOWNLOADING)
        return tmsDownloaderProgress(&upgrade->downloader);
    return upgrade->state == TMS_UPGRADE_COMPLETED ? 100u : 0u;
}

/** Check whether the upgrade has reached a terminal state. */
bool tmsUpgradeIsFinished(const TmsUpgrade* upgrade) {
    return upgrade != NULL && (upgrade->state == TMS_UPGRADE_COMPLETED ||
                               upgrade->state == TMS_UPGRADE_NO_UPDATE ||
                               upgrade->state == TMS_UPGRADE_CANCELLED ||
                               upgrade->state == TMS_UPGRADE_FAILED);
}
