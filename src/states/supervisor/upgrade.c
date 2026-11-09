/*
 * Upgrade state — UI integration for the already-tested full TMS flow.
 *
 * Replace the previous Upgrade state implementation with this adapted version,
 * but KEEP YOUR WORKING TMS PORT EXPRESSION (UPGRADE_TMS_PORT below), and
 * confirm TIMER_STOP/TIMER_DESTROY against your actual timer.h definitions.
 * These SDK-specific declarations were not included in the supplied files.
 *
 * DEV POS ONLY: this deliberately preserves your current package-specific
 * authorization and the existing DEV signature-skip setting in pnaTms.h.
 */
#include "upgrade.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "states.h"
#include "logger.h"
#include "sys/sys.h"
#include "settings/settings.h"
#include "utility/utility.h"
#include "ui/infoPage.h"
#include "phrases/phrases.h"
#include "timer.h"
#include "ui/progressBar.h"
#include "display/display.h"
#include "infoMgr/infoMgr.h"
#include "tms/vendor/pnaTms.h"
#include "tms/vendor/tmsBringup.h"
#include "version.h"

#define UPGRADE_STAGING_PREFIX "/mtd0/tms_compact_test"
#define UPGRADE_UI_PERIOD_MS   100u

static Timer* timer;
static State* upgradeParent; /* Captured in STATE_DEF_ENTER for async errors. */
static ProgressBar progressBar;
static bool        progressBarVisible;
static bool        noUpdateAvailable;
static bool        rebootMessageShownOnPreviousTick;
static int         lastUiPhase;
static unsigned    lastUiPercent;

typedef enum {
    UI_PHASE_CONNECTING = 0,
    UI_PHASE_CHECKING,
    UI_PHASE_DOWNLOADING,
    UI_PHASE_VERIFYING,
    UI_PHASE_PREPARING,
    UI_PHASE_RESTARTING,
    UI_PHASE_NO_UPDATE,
    UI_PHASE_CANCELED
} UpgradeUiPhase;

/* The old test's exact package. This is deliberately NOT production policy. */
static const uint8_t expectedPackageMd5[16] = {
    0xFC, 0xEB, 0xF3, 0x51, 0xC0, 0xB3, 0xF1, 0x8C,
    0xD6, 0x21, 0x86, 0xB0, 0xEF, 0xB2, 0x5D, 0xF3};

typedef struct {
    bool operatorApproved;
    bool exclusiveMaintenanceActive;
} TmsTestAuthorization;

/* Must outlive both asynchronous backend operations. */
static TmsTestAuthorization tmsAuthorization;

static bool authorizeTmsReplacement(void*                       context,
                                    const PnaTmsReplaceRequest* request) {
    TmsTestAuthorization* auth = (TmsTestAuthorization*)context;
    if (!auth || !request || !request->expectedMd5 || !auth->operatorApproved ||
        !auth->exclusiveMaintenanceActive || request->operationId == 0u ||
        request->stagedBytes != 1496248u || !request->destinationPath ||
        strcmp(request->destinationPath, "/mtd2/apupdate") != 0 ||
        !request->stagedPath ||
        strcmp(request->stagedPath, "/mtd0/tms_compact_test_00.bin") != 0 ||
        memcmp(request->expectedMd5, expectedPackageMd5,
               sizeof(expectedPackageMd5)) != 0)
        return false;

    auth->operatorApproved = false; /* Consume exactly once. */
    LOG_TRACE("TMS DEV: destination replacement authorized op=%llu",
              (unsigned long long)request->operationId);
    return true;
}

static void createDownloadBar(void) {
    if (progressBarVisible)
        return;
    ui_progress_bar_create(&progressBar, disp()->screen);
    ui_progress_bar_set_title(&progressBar, phraseGetDef(PHRASE_CHECK_UPG));
    ui_progress_bar_set_value(&progressBar, 0u);
    ui_progress_bar_show(&progressBar);
    progressBarVisible = true;
    lastUiPhase        = -1;
    lastUiPercent      = 101u;
}

static void destroyDownloadBar(void) {
    if (!progressBarVisible)
        return;
    ui_progress_bar_hide(&progressBar);
    ui_progress_bar_destroy(&progressBar);
    progressBarVisible = false;
}

/* Timer API names must be checked against your project-specific timer.h. */
static void stopUpgradeTimer(void) {
    if (timer)
        TIMER_STOP(timer);
}

static void destroyUpgradeTimer(void) {
    if (timer) {
        TIMER_STOP(timer);
        TIMER_DESTROY(timer);
        timer = NULL;
    }
}

static const char* uiTitle(UpgradeUiPhase phase) {
    switch (phase) {
    case UI_PHASE_CONNECTING:
        return phraseGetDef(PHRASE_TMS_CONNECTING);
    case UI_PHASE_CHECKING:
        return phraseGetDef(PHRASE_TMS_CHECKING);
    case UI_PHASE_DOWNLOADING:
        return phraseGetDef(PHRASE_TMS_DOWNLOADING);
    case UI_PHASE_VERIFYING:
        return phraseGetDef(PHRASE_TMS_VERIFYING);
    case UI_PHASE_PREPARING:
        return phraseGetDef(PHRASE_TMS_PREPARING);
    case UI_PHASE_RESTARTING:
        return phraseGetDef(PHRASE_TMS_RESTARTING);
    case UI_PHASE_NO_UPDATE:
        return phraseGetDef(PHRASE_TMS_NO_UPDATE);
    case UI_PHASE_CANCELED:
        return phraseGetDef(PHRASE_TMS_CANCELED);
    default:
        return phraseGetDef(PHRASE_TMS_UPDATING);
    }
}

static void paintProgress(UpgradeUiPhase phase, unsigned percent) {
    if (!progressBarVisible)
        return;
    if (percent > 99u)
        percent = 99u; /* Not installed until the next boot is confirmed. */
    /* This is PHASE progress, not an exact end-to-end byte/time percentage. */
    if (percent < lastUiPercent && lastUiPercent <= 99u &&
        phase != UI_PHASE_NO_UPDATE)
        percent = lastUiPercent;
    if ((int)phase != lastUiPhase) {
        ui_progress_bar_set_title(&progressBar, uiTitle(phase));
        lastUiPhase = (int)phase;
        LOG_TRACE("TMS UI: phase=%d", (int)phase);
    }
    if (percent != lastUiPercent) {
        ui_progress_bar_set_value(&progressBar, percent);
        lastUiPercent = percent;
    }
}

static void showUpgradeFailure(const TmsFullStatus* full) {
    if (full) {
        LOG_TRACE("TMS UI: failed stage=%d result=%d error=%d backend=%ld",
                  (int)full->stage, (int)full->last.result,
                  (int)full->last.error, (long)full->last.backendCode);
    }
    stopUpgradeTimer();
    destroyDownloadBar();
    /* Reuse the existing project's error/info navigation. */
    infoMgrsetCommon(upgradeParent, upgradeParent);
    infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                   phraseGetDef(PHRASE_UPDATE_FAILED), "");
}

static void refreshUpgradeUi(const TmsFullStatus* full) {
    if (!full)
        return;

    switch (full->stage) {
    case TMS_FULL_CHECKING:
        switch (full->last.state) {
        case TMS_STATE_CONNECTING:
        case TMS_STATE_STARTING:
            paintProgress(UI_PHASE_CONNECTING, 2u);
            break;
        case TMS_STATE_CHECKING:
            paintProgress(UI_PHASE_CHECKING, 5u);
            break;
        case TMS_STATE_DOWNLOADING:
            paintProgress(UI_PHASE_DOWNLOADING,
                          5u + (unsigned)full->last.progress * 75u / 100u);
            break;
        case TMS_STATE_VERIFYING:
            paintProgress(UI_PHASE_VERIFYING,
                          5u + (unsigned)full->last.progress * 75u / 100u);
            break;
        default:
            paintProgress(UI_PHASE_CHECKING, 5u);
            break;
        }
        break;
    case TMS_FULL_PREPARE_QUEUED:
        paintProgress(UI_PHASE_PREPARING, 83u);
        break;
    case TMS_FULL_PREPARING:
        paintProgress(UI_PHASE_PREPARING, 88u);
        break;
    case TMS_FULL_ACTIVATION_QUEUED:
        paintProgress(UI_PHASE_RESTARTING, 99u);
        /* Allow one FULL UI tick for LVGL to render this title. */
        rebootMessageShownOnPreviousTick = true;
        break;
    case TMS_FULL_REBOOT_REQUESTED:
        paintProgress(UI_PHASE_RESTARTING, 99u);
        break;
    case TMS_FULL_NO_UPDATE:
        noUpdateAvailable = true;
        stopUpgradeTimer();
        paintProgress(UI_PHASE_NO_UPDATE, 0u);
        LOG_TRACE("TMS UI: no update; ESC to exit");
        break;
    case TMS_FULL_CANCELED:
        /* Required only for future asynchronously cancelable backends;
         * current PNA cancel normally completes inside the ESC handler. */
        stopUpgradeTimer();
        paintProgress(UI_PHASE_CANCELED,
                      lastUiPercent <= 99u ? lastUiPercent : 0u);
        LOG_TRACE("TMS UI: canceled; ESC to exit");
        break;
    case TMS_FULL_FAILED:
        showUpgradeFailure(full);
        break;
    case TMS_FULL_IDLE:
    default:
        paintProgress(UI_PHASE_CHECKING, 0u);
        break;
    }
}

static void updateTick(void) {
    /* Never call UI APIs from an NTH worker thread. The Upgrade timer must
     * execute in the LVGL/UI execution context. nth()->tick() is pumped by
     * the existing application event loop as before. */

    /* The preceding tick displayed the Restarting title. Only NOW permit
     * the verified backend to set the update flag and reboot. */
    if (rebootMessageShownOnPreviousTick) {
        rebootMessageShownOnPreviousTick = false;
        (void)tmsBringupAllowFullActivation();
    }

    tmsBringupProcess();
    TmsFullStatus full = tmsBringupGetFullStatus();
    refreshUpgradeUi(&full);
}

STATE_DEF_ENTER(Upgrade) {
    upgradeParent                    = state->parent;
    noUpdateAvailable                = false;
    rebootMessageShownOnPreviousTick = false;
    tmsAuthorization                 = (TmsTestAuthorization){0};
    timer                            = NULL;
    char communicationIp[32]         = {0};
    normalizeIp(settings()->server.tmsIp, communicationIp,
                sizeof(communicationIp));

    PnaTmsIdentity identity;
    pnaTmsIdentityClear(&identity);

    DEFINE_STRING(sn, 32);
    memset(sn, 0, sizeof(sn));
    OOP_CALL(sys(), getSN, sn, sizeof(sn));

    bool valid = true;
    valid &= pnaTmsCopyField(identity.model, sizeof(identity.model),
                             POS_MODEL_NAME, strlen(POS_MODEL_NAME) + 1u);
    valid &= pnaTmsCopyField(identity.serial, sizeof(identity.serial), sn,
                             sizeof(sn));
    valid &= pnaTmsCopyField(identity.terminal, sizeof(identity.terminal),
                             settings()->terminal.terminalId,
                             sizeof(settings()->terminal.terminalId));
    valid &= pnaTmsCopyField(identity.appVersion, sizeof(identity.appVersion),
                             PNA_APP_VERSION, strlen(PNA_APP_VERSION) + 1u);

    if (!valid) {
        LOG_TRACE("TMS UI: invalid device identity");
        showUpgradeFailure(NULL);
        return;
    }

    /* DEV test only: retain your previously verified authorization setup.
     * This does NOT actually obtain operator consent or a maintenance lock. */
    tmsAuthorization.operatorApproved           = true;
    tmsAuthorization.exclusiveMaintenanceActive = true;

    /* Use the same tested port and the existing explicit READY discard. */
    const TmsFullConfig cfg = {
        .host                 = communicationIp,
        .port                 = (uint16_t)(settings()->server.tmsPort),
        .useTls               = false,
        .identity             = &identity,
        .stagingPrefix        = UPGRADE_STAGING_PREFIX,
        .authorizeReplace     = authorizeTmsReplacement,
        .authorizationContext = &tmsAuthorization,
        .discardExistingReady = true /* DEV only: force fresh download */
    };

    /* Allocate the pump before accepting any asynchronous operation.
     * Otherwise a timer-allocation failure would strand an active backend. */
    timer = TIMER_CREATE(updateTick, UPGRADE_UI_PERIOD_MS, false);
    if (!timer) {
        LOG_TRACE("TMS UI: timer allocation failed; upgrade not started");
        showUpgradeFailure(NULL);
        return;
    }

    const bool accepted = tmsBringupStartFullUpgrade(&cfg);
    LOG_TRACE("TMS FULL UI: start accepted=%d", (int)accepted);
    if (!accepted) {
        showUpgradeFailure(NULL);
        return;
    }

    createDownloadBar();
    refreshUpgradeUi(&(TmsFullStatus){.stage      = TMS_FULL_CHECKING,
                                      .last.state = TMS_STATE_STARTING});
    TIMER_START(timer);
}

STATE_DEF_EXIT(Upgrade) {
    destroyUpgradeTimer();
    destroyDownloadBar();
    rebootMessageShownOnPreviousTick = false;
    /* In normal terminal cases no backend operation remains active.
     * Do not navigate away while a protected updater write is running. */
}

STATE_DEF_HANDLE(Upgrade, KeypadEvent) {
    if (!ev || ev->key != KEY_ESC)
        return;

    TmsFullStatus full = tmsBringupGetFullStatus();
    if (noUpdateAvailable || full.stage == TMS_FULL_NO_UPDATE ||
        full.stage == TMS_FULL_CANCELED) {
        infoMgrsetCommon(upgradeParent, upgradeParent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_TMS_CANCELED), "");
        return;
    }

    /* First ESC requests cancellation. The backend may reject it once an
     * APUPDATE write has begun; in that case we MUST keep pumping. */
    if (!tmsBringupCancelFullUpgrade()) {
        LOG_TRACE("TMS UI: cancel unavailable/unsafe stage=%d state=%d",
                  (int)full.stage, (int)full.last.state);
        return;
    }

    /* Current PNA backend completes cancellation synchronously. Finish its
     * deferred backend cleanup before leaving the state and its pump. */
    full = tmsBringupGetFullStatus();
    if (full.stage == TMS_FULL_CANCELED) {
        tmsBringupProcess(); /* Release idle backend; cannot start next step. */
        LOG_TRACE("TMS UI: canceled; leaving Upgrade state");
        infoMgrsetCommon(upgradeParent, upgradeParent);
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_TMS_CANCELED), "");
        return;
    }

    /* Future async backends: keep the timer active until CANCELED arrives.
     * refreshUpgradeUi() will display canceled and stop the timer; ESC
     * then closes that screen. Never leave while still active. */
    LOG_TRACE("TMS UI: cancellation pending");
}

OOP_CTOR(Upgrade, State* parent, const char* name) {
    OOP_CALL_CTOR(State, self, parent, name);
    self->base.vtable.enter        = STATE_ENTER(Upgrade);
    self->base.vtable.exit         = STATE_EXIT(Upgrade);
    self->base.vtable.handleKeypad = STATE_HANDLE(Upgrade, KeypadEvent);
}
