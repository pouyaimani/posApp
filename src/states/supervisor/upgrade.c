#include "upgrade.h"
#include "sys/sys.h"
#include "file/file.h"
#include "version.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "tms/tmsUpgrade.h"
#include "tms/tmsPlatform.h"
#include "ui/infoPage.h"
#include "phrases/phrases.h"
#include "timer.h"
#include "ui/downloadProgress.h"
#include "display/display.h"

#define BRAND_CODE "PNAP_3300"

#define TMS_HOST "sabztms.ir"
#define TMS_PORT 18094U

/* Confirm this value with the TMS backend contract. */
#define TMS_RESOURCE_APPLICATION 1

static TmsUpgrade* tmsUpgrade;
static Timer*      timer;

static DownloadProgress* progressBar;

static bool isBatteryOk() {
    return OOP_CALL(sys(), getBatteryStatus)->level > DEV_BAT_LEV_1 ? true
                                                                    : false;
}

static void tmsUpgradeFinished(TmsUpgrade* upgrade, TmsUpgradeResult result,
                               void* userData) {
    (void)upgrade;
    State* state = (State*)userData;

    switch (result) {
    case TMS_UPGRADE_RESULT_OK:
        LOG_DEBUG("TMS update completed");
        /*
         * Current implementation reboots immediately
         * after this callback returns.
         */
        break;

    case TMS_UPGRADE_RESULT_NO_UPDATE:
        LOG_DEBUG("No TMS update available");
        break;

    default:
        LOG_ERROR("TMS update failed: %d", result);
        break;
    }
    TIMER_REMOVE(timer);
    MEM_FREE(tmsUpgrade);
    ui_download_progress_destroy(progressBar);
    SM_GOTO(state->parent);
}

void processUpdate(void) {
    tmsUpgradeProcess(tmsUpgrade);
    int percent = tmsUpgradeProgress(tmsUpgrade);
    LOG_DEBUG("upgrade percent = %d", percent);

    ui_download_progress_set_value(progressBar, percent, 100);

    ui_download_progress_set_size(progressBar, 0, 3600);
}

STATE_DEF_ENTER(Upgrade) {
    tmsUpgrade = MEM_ALLOC(sizeof(TmsUpgrade));
    RETURN_IF_NULL(tmsUpgrade,
                   GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                             phraseGetDef(PHRASE_UPG_START_FAILED), ""););
    tmsUpgradeInit(tmsUpgrade, TMS_HOST, TMS_PORT, tmsPlatformOps(),
                   tmsUpgradeFinished, state);

    DEFINE_STRING(appVersion, 128);
    DEFINE_STRING(sn, 128);
    sprintf(appVersion, "V%s", PNA_TMS_VERSION);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));

    TmsUpgradeResult result = tmsUpgradeStart(
        tmsUpgrade, sn, BRAND_CODE, appVersion, TMS_RESOURCE_APPLICATION);

    if (result != TMS_UPGRADE_RESULT_OK) {
        LOG_ERROR("Could not start TMS update: %d", result);
        DEFINE_STRING(errDsc, 128);
        snprintf(errDsc, sizeof(errDsc), "%s = %d",
                 phraseGetDef(PHRASE_ERR_CODE), result);
        GOTO_INFO(state->parent, state->parent, INFO_ERROR,
                  phraseGetDef(PHRASE_UPG_START_FAILED), errDsc);
        return;
    }
    timer       = TIMER_CREATE(processUpdate, SECS(1), false);
    progressBar = ui_download_progress_create(disp()->screen, 16, 70, 288);

    ui_download_progress_set_title(progressBar, "Downloading update...");

    ui_download_progress_set_value(progressBar, 0, 100);

    ui_download_progress_set_size(progressBar, 0, 3600);
}

OOP_CTOR(Upgrade, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Upgrade);
}