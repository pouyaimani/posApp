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

#define BRAND_CODE "PNAP_3300"

#define TMS_HOST "sabztms.ir"
#define TMS_PORT 18094U

/* Confirm this value with the TMS backend contract. */
#define TMS_RESOURCE_APPLICATION 1

// static TmsUpgrade* tmsUpgrade;
static Timer* timer;

static ProgressBar progressBar;

static uint32_t last_percent = 0;

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
    unsigned percent = (unsigned)((uint64_t)written * 100 / total);
    (void)user;
    if (percent >= last_percent + 1 || percent == 100) {
        LOG_TRACE("TMS download: %u%% (%lu/%lu)", percent,
                  (unsigned long)written, (unsigned long)total);
        last_percent = percent;

        ui_progress_bar_set_value(&progressBar, percent);
    }
}

void updateTick(void) { tmsDownloadProbeTick(tmsDownloadProgress); }

static void upgCheckDone(const TmsCheckResult* r, void* user) {
    State* state = user;
    LOG_TRACE("TMS check: result=%d HTTP=%u transport=%d parser=%d",
              (int)r->result, r->http.http_status, (int)r->http.result,
              (int)r->http.http_result);
    if (r->has_server_code)
        LOG_TRACE("TMS server: code=%ld message=%s", (long)r->server_code,
                  r->upgrade.message);
    if (r->result != TMS_API_OK) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
        return;
    }
    if (!r->upgrade.has_update) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_WARNING, &infoFinishCtx,
                       phraseGetDef(PHRASE_NO_UPDATE), "");
        return;
    }
    LOG_TRACE("TMS: update available version=%s size=%lu", r->upgrade.version,
              (unsigned long)r->upgrade.file_size);
    /* Metadata is available in r->upgrade. No download/install is started. */
    if (tmsDownloadProbeInit() != TMS_DL_OK) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
        return;
    }
    LOG_TRACE("TMS URL: length=%lu prefix=%.8s",
              (unsigned long)strlen(r->upgrade.url), r->upgrade.url);

    TmsDownloadResult rc =
        tmsDownloadProbeQueue(r->upgrade.url, r->upgrade.file_size);
    if (rc != TMS_DL_OK) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
        return;
    }
    HIDE_INFO();

    createDownloadBar();

    last_percent = 0;

    timer = TIMER_CREATE(updateTick, SECS(1), false);
    TIMER_START(timer);

    LOG_TRACE("TMS download queued: result=%d", (int)rc);
}

STATE_DEF_ENTER(Upgrade) {
    DEFINE_STRING(appVersion, 128);
    DEFINE_STRING(sn, 128);
    sprintf(appVersion, "V%s", PNA_TMS_VERSION);
    OOP_CALL(sys(), getSN, sn, sizeof(sn));

    if (tmsUpgradeCheckStart(TMS_HOST, /* Same host as your successful probe */
                             TMS_PORT, sn, BRAND_CODE,
                             appVersion, /* Same version format as legacy TMS */
                             upgCheckDone, state) != TMS_API_OK) {
        infoMgrsetCommon(state->parent, state->parent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UPDATE_FAILED), "");
        return;
    }
    SHOW_INFO(INFO_WAITING, phraseGetDef(PHRASE_CHECK_UPG), "");
}

STATE_DEF_HANDLE(Upgrade, KeypadEvent) {
    if (ev->key == KEY_ESC) {
        tmsUpgradeCheckCancel();
        tmsDownloadProbeCancel();
        destroyDownloaadBar();
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