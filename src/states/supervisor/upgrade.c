#include "upgrade.h"
#include "sys/sys.h"
#include "file/file.h"
#include "version.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "tms/tmsTimeSync.h"
#include "tms/tmsUpgradeCheck.h"
#include "ui/infoPage.h"
#include "phrases/phrases.h"
#include "timer.h"
#include "ui/downloadProgress.h"
#include "display/display.h"
#include "infoMgr/infoMgr.h"

#define BRAND_CODE "PNAP_3300"

#define TMS_HOST "sabztms.ir"
#define TMS_PORT 18094U

/* Confirm this value with the TMS backend contract. */
#define TMS_RESOURCE_APPLICATION 1

// static TmsUpgrade* tmsUpgrade;
static Timer* timer;

static DownloadProgress* progressBar;

static bool isBatteryOk() {
    return OOP_CALL(sys(), getBatteryStatus)->level > DEV_BAT_LEV_1 ? true
                                                                    : false;
}

// static void tmsUpgradeFinished(TmsUpgrade* upgrade, TmsUpgradeResult result,
//                                void* userData) {
//     (void)upgrade;
//     State* state = (State*)userData;

//     TIMER_REMOVE(timer);
//     MEM_FREE(tmsUpgrade);
//     ui_download_progress_destroy(progressBar);
//     switch (result) {
//     case TMS_UPGRADE_RESULT_OK:
//         LOG_DEBUG("TMS update completed");
//         /*
//          * Current implementation reboots immediately
//          * after this callback returns.
//          */
//         infoMgrsetCommon(state->parent, state->parent);
//         infoMgr()->run(INFO_SUCCESS, &infoFinishCtx,
//                        phraseGetDef(PHRASE_UPDATE_SUCCEED), "");
//         break;

//     case TMS_UPGRADE_RESULT_NO_UPDATE:
//         infoMgrsetCommon(state->parent, state->parent);
//         infoMgr()->run(INFO_WARNING, &infoFinishCtx,
//                        phraseGetDef(PHRASE_NO_UPDATE), "");
//         break;

//     default:
//         infoMgrsetCommon(state->parent, state->parent);
//         infoMgr()->run(INFO_ERROR, &infoFinishCtx,
//                        phraseGetDef(PHRASE_UPDATE_FAILED), "");
//         break;
//     }
// }

// void processUpdate(void) {
//     tmsUpgradeProcess(tmsUpgrade);
//     int percent = tmsUpgradeProgress(tmsUpgrade);
//     LOG_DEBUG("upgrade percent = %d", percent);
//     if (percent != 0) {
//         ui_download_progress_show(progressBar, true);

//         ui_download_progress_set_value(progressBar, percent, 100);

//         ui_download_progress_set_size(progressBar, 0, 3600);
//     }
// }

STATE_DEF_ENTER(Upgrade) {
    // tmsUpgrade = MEM_ALLOC(sizeof(TmsUpgrade));
    // infoMgrsetCommon(state->parent, state->parent);
    // RETURN_IF_NULL(tmsUpgrade,
    //                infoMgr()->run(INFO_ERROR, &infoFinishCtx,
    //                               phraseGetDef(PHRASE_UPG_START_FAILED),
    //                               ""););
    // tmsUpgradeInit(tmsUpgrade, TMS_HOST, TMS_PORT, tmsPlatformOps(),
    //                tmsUpgradeFinished, state);

    // DEFINE_STRING(appVersion, 128);
    // DEFINE_STRING(sn, 128);
    // sprintf(appVersion, "V%s", PNA_TMS_VERSION);
    // OOP_CALL(sys(), getSN, sn, sizeof(sn));

    // TmsUpgradeResult result = tmsUpgradeStart(
    //     tmsUpgrade, sn, BRAND_CODE, appVersion, TMS_RESOURCE_APPLICATION);

    // if (result != TMS_UPGRADE_RESULT_OK) {
    //     LOG_ERROR("Could not start TMS update: %d", result);
    //     MEM_FREE(tmsUpgrade);
    //     DEFINE_STRING(errDsc, 128);
    //     snprintf(errDsc, sizeof(errDsc), "%s = %d",
    //              phraseGetDef(PHRASE_ERR_CODE), result);
    //     infoMgrsetCommon(state->parent, state->parent);
    //     infoMgr()->run(INFO_ERROR, &infoFinishCtx,
    //                    phraseGetDef(PHRASE_UPG_START_FAILED), errDsc);
    //     return;
    // }
    // timer = TIMER_CREATE(processUpdate, SECS(1), false);
    // TIMER_START(timer);
    // progressBar = ui_download_progress_create(disp()->screen, 16, 70, 288);

    // ui_download_progress_set_title(progressBar, "Downloading update...");

    // ui_download_progress_set_value(progressBar, 0, 100);

    // ui_download_progress_set_size(progressBar, 0, 3600);

    // ui_download_progress_show(progressBar, false);

    DEFINE_STRING(appVersion, 128);
    DEFINE_STRING(sn, 128);
    sprintf(appVersion, "V%s", PNA_TMS_VERSION);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));

    tmsUpgradeCheckStart(TMS_HOST, /* Same host as your successful probe */
                         TMS_PORT, sn, BRAND_CODE,
                         appVersion /* Same version format as legacy TMS */
    );

    // HttpClientResult result = tmsHttpProbeStart(TMS_HOST, TMS_PORT);
}

OOP_CTOR(Upgrade, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter = STATE_ENTER(Upgrade);
}