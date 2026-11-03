#include "upgrade.h"
#include "sys/sys.h"
#include "file/file.h"
#include "version.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "tms/tmsTimeSync.h"
#include "tms/tmsUpgradeCheck.h"
#include "tms/tmsDownload.h"
#include "tms/tmsDownloadProbe.h"
#include "ui/infoPage.h"
#include "phrases/phrases.h"
#include "timer.h"
#include "ui/progressBar.h"
#include "display/display.h"
#include "infoMgr/infoMgr.h"
#include "tms1/vendor/pnaTms.h"
#include "tms1/tms.h"
#include "tms1/vendor/tmsBringUp.h"
#include "version.h"

/* Confirm this value with the TMS backend contract. */
#define TMS_RESOURCE_APPLICATION 1

// static TmsUpgrade* tmsUpgrade;
static Timer* timer;

static ProgressBar progressBar;

static void createDownloadBar() {
    ui_progress_bar_create(&progressBar, disp()->screen);

    ui_progress_bar_set_title(&progressBar, phraseGetDef(PHRASE_UPDATING));

    ui_progress_bar_set_value(&progressBar, 0);

    ui_progress_bar_show(&progressBar);
}

static void destroyDownloaadBar() {
    ui_progress_bar_hide(&progressBar);
    ui_progress_bar_destroy(&progressBar);
}

static bool isBatteryOk() {
    return OOP_CALL(sys(), getBatteryStatus)->level > DEV_BAT_LEV_1 ? true
                                                                    : false;
}

void tmsDownloadProgress(uint32_t written, uint32_t total, void* user) {
    // unsigned percent = (unsigned)((uint64_t)written * 100 / total);
    // (void)user;
    // if (percent >= last_percent + 1 || percent == 100) {
    //     LOG_TRACE("TMS download: %u%% (%lu/%lu)", percent,
    //               (unsigned long)written, (unsigned long)total);
    //     last_percent = percent;

    //     ui_progress_bar_set_value(&progressBar, percent);
    // }
}

void updateTick(void) { tmsBringupProcess(); }

STATE_DEF_ENTER(Upgrade) {
    char communicationIp[32];
    normalizeIp(settings()->server.tmsIp, communicationIp,
                sizeof(communicationIp));
    //                 typedef struct {
    //     uint8_t model[16];
    //     uint8_t serial[30];
    //     uint8_t terminal[8];
    //     uint8_t appVersion[16];
    //     uint8_t biosVersion[16];
    //     uint8_t hwInfo[100];
    // } PnaTmsIdentity;

    PnaTmsIdentity identity;
    pnaTmsIdentityClear(&identity);

    DEFINE_STRING(sn, 32);
    memset(sn, 0, sizeof(sn));

    /* Read serial number from device HAL. */
    OOP_CALL(sys(), getSN, sn, sizeof(sn));

    bool valid = true;

    valid &= pnaTmsCopyField(identity.model, sizeof(identity.model),
                             POS_MODEL_NAME, strlen(POS_MODEL_NAME) + 1);

    valid &= pnaTmsCopyField(identity.serial, sizeof(identity.serial), sn,
                             sizeof(sn));

    valid &= pnaTmsCopyField(identity.terminal, sizeof(identity.terminal),
                             settings()->terminal.terminalId,
                             sizeof(settings()->terminal.terminalId));

    valid &= pnaTmsCopyField(identity.appVersion, sizeof(identity.appVersion),
                             PNA_APP_VERSION, strlen(PNA_APP_VERSION) + 1);

    if (!valid) {
        LOG_TRACE("TMS: invalid device identity");
        return;
    }

    /* BIOS and HW information remain space-filled. */
    bool accepted =
        tmsBringupStartAllFilesMd5(communicationIp, settings()->server.tmsPort,
                                   0, &identity, "/mtd0/tms_stage10_probe");

    LOG_TRACE("TMS STAGE7: accepted=%d", accepted);

    if (!accepted) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
        return;
    }
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CHECK_UPG), "");
    // createDownloadBar();

    // last_percent = 0;

    updateTick();

    timer = TIMER_CREATE(updateTick, 100, false);
    TIMER_START(timer);
}

STATE_DEF_HANDLE(Upgrade, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
    }
}

OOP_CTOR(Upgrade, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter        = STATE_ENTER(Upgrade);
    self->base.vtable.handleKeypad = STATE_HANDLE(Upgrade, KeypadEvent);
}