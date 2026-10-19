#include "nfcMgr.h"

#include <stddef.h>

#include "logger.h"
#include "mylvgl.h"
#include "display.h"
#include "ui/button.h"
#include "utility/utility.h"
#include "phrases/phrases.h"
#include "font/myFont.h"
#include "sys/sys.h"
#include "error.h"
#include "assets.h"

/* --------------------------------------------------------------------------
 * Private manager state
 * -------------------------------------------------------------------------- */

static NfcMgr manager; /**< Singleton API. */
static NfcCfg request; /**< Copied settings; label strings remain borrowed. */

static NfcComplete callback;     /**< Current completion callback. */
static void*       callbackData; /**< Borrowed application context. */

static Picc* reader; /**< HAL singleton, exclusively leased while active. */

static uint32_t scanId; /**< Rejects queued events from older scan sessions. */

static bool initialized; /**< State and UI have been constructed. */
static bool active;      /**< An accepted request has not completed/aborted. */
static bool entered;     /**< NFC state has entered and has not yet exited. */
static bool ownsReader;  /**< Reader must be closed by this manager. */

/* NFC screen objects. */
static lv_obj_t* iconLabel; /**< Large NFC marker. */
static lv_obj_t* infoLabel; /**< Prompt or multiple-card warning. */

/* --------------------------------------------------------------------------
 * UI helpers
 * -------------------------------------------------------------------------- */

static void info_opa_set(void* obj, int32_t opa) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)opa, 0);
}

static void info_start_pulse(void) {
    lv_anim_t anim;
    lv_anim_init(&anim);

    lv_anim_set_var(&anim, infoLabel);
    lv_anim_set_exec_cb(&anim, info_opa_set);
    lv_anim_set_values(&anim, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&anim, 900);
    lv_anim_set_playback_time(&anim, 900);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&anim);
}

static void info_stop_pulse(void) {
    lv_anim_del(infoLabel, info_opa_set);
    lv_obj_set_style_opa(infoLabel, LV_OPA_COVER, 0);
}

/** Hide all manager-owned screen objects. */
static void ui_hide(void) {
    LOG_TRACE("nfcMgr: ui_hide");

    info_stop_pulse();

    LV_HIDE(iconLabel);
    LV_HIDE(infoLabel);
}

/**
 * Show the acquisition prompt, with a warning when multiple cards are
 * detected.
 */
static void ui_show(bool multiple) {
    const char* info =
        multiple
            ? (request.multipleInfo ? request.multipleInfo
                                    : "Present only one card")
            : (request.info ? request.info : phraseGetDef(PHRASE_TAP_PHONE));

    LOG_TRACE("nfcMgr: ui_show multiple=%d", (int)multiple);

    LV_SET_TEXT(infoLabel, info);

    LV_SHOW(iconLabel);
    LV_SHOW(infoLabel);
    info_start_pulse();
}

/** Build a minimal screen using the same UI helpers as inputMgr. */
static void ui_create(void) {
    LOG_TRACE("nfcMgr: ui_create begin");

    iconLabel = lv_img_create(disp()->screen);
    lv_img_set_src(iconLabel, ICON_TAP_PHONE);
    LV_ALIGN(iconLabel, LV_ALIGN_CENTER, 0, -30);

    infoLabel = lv_label_create(disp()->screen);
    LV_SET_TEXT_FONT(infoLabel, FONT_20);
    LV_SET_TEXT_COLOR(infoLabel, COLOR_TEXT_SECONDARY);
    LV_ALIGN(infoLabel, LV_ALIGN_BOTTOM_MID, 0, -15);

    ui_hide();

    LOG_TRACE("nfcMgr: ui_create complete");
}

/* --------------------------------------------------------------------------
 * Reader cleanup
 * -------------------------------------------------------------------------- */

/**
 * Stop detection before closing.
 * Keep ownership if close fails for a later retry.
 */
static PiccErr_t closeOwnedReader(void) {
    LOG_TRACE("nfcMgr: closeOwnedReader begin ownsReader=%d reader=%p",
              (int)ownsReader, (void*)reader);

    if (!ownsReader || !reader) {
        LOG_TRACE(
            "nfcMgr: closeOwnedReader skipped ownsReader=%d readerPresent=%d",
            (int)ownsReader, reader != NULL);
        return PICC_ERR_OK;
    }

    LOG_TRACE("nfcMgr: stopping PICC polling before reader close");

    piccStopPolling(reader);

    LOG_TRACE("nfcMgr: calling reader close");

    PiccErr_t error = OOP_CALL(reader, close);

    if (error == PICC_ERR_OK) {
        ownsReader = false;

        LOG_TRACE("nfcMgr: reader closed successfully; ownership released");
    } else {
        LOG_TRACE("nfcMgr: reader close failed error=%d; ownership retained",
                  (int)error);
    }

    LOG_TRACE("nfcMgr: closeOwnedReader end error=%d ownsReader=%d", (int)error,
              (int)ownsReader);

    return error;
}

/* --------------------------------------------------------------------------
 * Request completion
 * -------------------------------------------------------------------------- */

/**
 * Finish once, snapshot event data before HAL close resets it, then notify.
 */
static void finish(NfcResult result, PiccErr_t error, const PiccData_t* card) {
    LOG_TRACE("nfcMgr: finish requested result=%d error=%d card=%p active=%d",
              (int)result, (int)error, (const void*)card, (int)active);

    if (!active) {
        LOG_TRACE("nfcMgr: finish ignored; no active request");
        return;
    }

    /* Mark the request complete before performing cleanup or callbacks. */
    active = false;

    LOG_TRACE("nfcMgr: request marked inactive");

    /* Snapshot the callback before clearing the current request context. */
    NfcComplete done     = callback;
    void*       doneData = callbackData;

    callback     = NULL;
    callbackData = NULL;

    LOG_TRACE("nfcMgr: completion callback present=%d", done != NULL);

    /* Build the result before closing the reader. */
    NfcStatus status = {0};

    status.result        = result;
    status.error         = error;
    status.card.cardType = PICC_CARD_NONE;

    if (card) {
        status.card = *card;

        LOG_TRACE("nfcMgr: card data captured cardType=%d",
                  (int)status.card.cardType);
    } else {
        LOG_TRACE("nfcMgr: no card data supplied");
    }

    /* Stop polling before either transferring or releasing ownership. */
    if (ownsReader) {
        LOG_TRACE("nfcMgr: stopping polling during request completion");

        piccStopPolling(reader);
    }

    if (result == NFC_RES_DETECTED && ownsReader && request.keepOpenOnSuccess) {

        /*
         * Ownership transfers now; state exit must not close the reader.
         */
        ownsReader            = false;
        status.readerRetained = true;

        LOG_TRACE("nfcMgr: reader ownership transferred to caller");
    } else {
        LOG_TRACE("nfcMgr: closing owned reader after result=%d", (int)result);

        status.cleanupError = closeOwnedReader();

        if (result == NFC_RES_DETECTED && status.cleanupError != PICC_ERR_OK) {

            LOG_TRACE("nfcMgr: reader cleanup failed after detection "
                      "cleanupError=%d; converting result to error",
                      (int)status.cleanupError);

            status.result = NFC_RES_ERROR;
            status.error  = status.cleanupError;
        }
    }

    status.readerOpen = reader && reader->opened;

    LOG_TRACE("nfcMgr: reader state after cleanup open=%d ownsReader=%d",
              (int)status.readerOpen, (int)ownsReader);

    ui_hide();

    LOG_TRACE("nfcMgr: completed result=%d card=%d error=%d "
              "cleanup=%d retained=%d readerOpen=%d",
              (int)status.result, (int)status.card.cardType, (int)status.error,
              (int)status.cleanupError, (int)status.readerRetained,
              (int)status.readerOpen);

    if (done) {
        LOG_TRACE("nfcMgr: invoking completion callback");

        done(&status, doneData);

        LOG_TRACE("nfcMgr: completion callback returned");
    } else {
        LOG_TRACE("nfcMgr: no completion callback to invoke");
    }
}

/* --------------------------------------------------------------------------
 * NFC state lifecycle
 * -------------------------------------------------------------------------- */

/** Open and power the reader only when this state becomes current. */
STATE_DEF_ENTER(Nfc) {
    LOG_TRACE("nfcMgr: state enter active=%d reader=%p", (int)active,
              (void*)reader);

    entered = true;

    if (!active) {
        LOG_TRACE("nfcMgr: state enter ignored; no active request");
        return;
    }

    ui_show(false);

    /*
     * run() checked this too; recheck in case state entry was deferred.
     */
    if (reader->opened) {
        LOG_TRACE("nfcMgr: state enter failed; reader is already open");

        finish(NFC_RES_ERROR, PICC_ERR_OTHER, NULL);
        return;
    }

    LOG_TRACE("nfcMgr: initializing reader");

    PiccErr_t error = OOP_CALL(reader, init);

    if (error != PICC_ERR_OK) {
        LOG_TRACE("nfcMgr: reader initialization failed error=%d", (int)error);

        finish(NFC_RES_ERROR, error, NULL);
        return;
    }

    ownsReader = true;

    LOG_TRACE("nfcMgr: reader initialized; ownership acquired");

    LOG_TRACE("nfcMgr: powering on reader technologies=%lu",
              (unsigned long)request.technologies);

    error = OOP_CALL(reader, powerOn, request.technologies);

    if (error != PICC_ERR_OK) {
        LOG_TRACE("nfcMgr: reader power-on failed error=%d", (int)error);

        finish(NFC_RES_ERROR, error, NULL);
        return;
    }

    LOG_TRACE("nfcMgr: reader powered on successfully");

    LOG_TRACE("nfcMgr: starting PICC polling");

    error = piccStartPolling(reader);

    if (error != PICC_ERR_OK) {
        LOG_TRACE("nfcMgr: failed to start PICC polling error=%d", (int)error);

        finish(NFC_RES_ERROR, error, NULL);
        return;
    }

    scanId = reader->scanId;

    LOG_TRACE("nfcMgr: scanning started technologies=%lu scan=%lu",
              (unsigned long)request.technologies, (unsigned long)scanId);
}

/**
 * Abort silently on external navigation; retained readers belong to the
 * caller.
 */
STATE_DEF_EXIT(Nfc) {
    LOG_TRACE("nfcMgr: state exit active=%d entered=%d ownsReader=%d",
              (int)active, (int)entered, (int)ownsReader);

    active       = false;
    entered      = false;
    callback     = NULL;
    callbackData = NULL;

    PiccErr_t error = closeOwnedReader();

    if (error != PICC_ERR_OK) {
        LOG_TRACE("nfcMgr: state exit reader cleanup failed error=%d",
                  (int)error);
    }

    ui_hide();

    LOG_TRACE("nfcMgr: state exit complete ownsReader=%d", (int)ownsReader);
}

/* --------------------------------------------------------------------------
 * PICC event handling
 * -------------------------------------------------------------------------- */

/**
 * Consume a snapshot from the active scan; multiple cards are nonterminal.
 */
STATE_DEF_HANDLE(Nfc, PiccEvent) {
    if (!active || !entered || !ownsReader) {
        LOG_TRACE("nfcMgr: PICC event ignored; invalid state "
                  "active=%d entered=%d ownsReader=%d",
                  (int)active, (int)entered, (int)ownsReader);
        return;
    }

    if (!ev) {
        LOG_TRACE("nfcMgr: PICC event ignored; event pointer is NULL");
        return;
    }

    if (!ev->data) {
        LOG_TRACE("nfcMgr: PICC event ignored; data is NULL scan=%lu",
                  (unsigned long)ev->scanId);
        return;
    }

    if (ev->scanId != scanId) {
        LOG_TRACE("nfcMgr: stale PICC event ignored "
                  "eventScan=%lu activeScan=%lu",
                  (unsigned long)ev->scanId, (unsigned long)scanId);
        return;
    }

    LOG_TRACE("nfcMgr: PICC event received scan=%lu cardType=%d error=%d",
              (unsigned long)ev->scanId, (int)ev->data->cardType,
              (int)ev->error);

    if (ev->error != PICC_ERR_OK) {
        LOG_TRACE("nfcMgr: PICC event reported error=%d", (int)ev->error);

        finish(NFC_RES_ERROR, ev->error, NULL);
        return;
    }

    switch (ev->data->cardType) {
    case PICC_CARD_NONE:
        LOG_TRACE("nfcMgr: no card detected; continuing scan");

        ui_show(false);
        return;

    case PICC_CARD_MULTIPLE:
        LOG_TRACE("nfcMgr: multiple cards detected; requesting one card");

        ui_show(true);
        return;

    case PICC_CARD_A:
    case PICC_CARD_B:
    case PICC_CARD_A_MEMORY:
    case PICC_CARD_B_MEMORY:
    case PICC_CARD_FELICA:
        LOG_TRACE("nfcMgr: supported card detected cardType=%d",
                  (int)ev->data->cardType);

        finish(NFC_RES_DETECTED, PICC_ERR_OK, ev->data);
        return;

    case PICC_CARD_UNKNOWN:
    default:
        LOG_TRACE("nfcMgr: unsupported or unknown card type=%d",
                  (int)ev->data->cardType);

        finish(NFC_RES_ERROR, PICC_ERR_OTHER, NULL);
        return;
    }
}

/* --------------------------------------------------------------------------
 * Keypad event handling
 * -------------------------------------------------------------------------- */

/** ESC cancels. ENTER cannot manufacture a successful detection. */
STATE_DEF_HANDLE(Nfc, KeypadEvent) {
    if (!ev) {
        LOG_TRACE("nfcMgr: keypad event ignored; event pointer is NULL");
        return;
    }

    LOG_TRACE("nfcMgr: keypad event received key=%d active=%d entered=%d",
              (int)ev->key, (int)active, (int)entered);

    if (ev->key == KEY_ESC) {
        LOG_TRACE("nfcMgr: ESC pressed; canceling NFC request");

        finish(NFC_RES_CANCELED, PICC_ERR_OK, NULL);
        return;
    }

    LOG_TRACE("nfcMgr: keypad event ignored; key does not cancel NFC request");
}

/* --------------------------------------------------------------------------
 * Timeout event handling
 * -------------------------------------------------------------------------- */

/** Acquisition timeout is distinct from explicit user cancellation. */
STATE_DEF_HANDLE(Nfc, TimeOutEvent) {
    LOG_TRACE("nfcMgr: timeout event entered=%d active=%d", (int)entered,
              (int)active);

    if (entered) {
        LOG_TRACE("nfcMgr: acquisition timed out");

        finish(NFC_RES_TIMEOUT, PICC_ERR_TIMEOUT, NULL);
    } else {
        LOG_TRACE("nfcMgr: timeout event ignored; NFC state has not entered");
    }
}

/* --------------------------------------------------------------------------
 * Public manager operations
 * -------------------------------------------------------------------------- */

/** Start one exclusive acquisition request; invalid/busy calls are rejected. */
static bool run(const NfcCfg* cfg, NfcComplete cb, void* userData) {
    LOG_TRACE("nfcMgr: run requested cfg=%p callbackPresent=%d userData=%p",
              (const void*)cfg, cb != NULL, userData);

    const PiccTechMask_t supported =
        PICC_TECH_A | PICC_TECH_B | PICC_TECH_FELICA;

    if (!cfg) {
        LOG_TRACE("nfcMgr: run rejected; configuration is NULL");
        return false;
    }

    if (!cb) {
        LOG_TRACE("nfcMgr: run rejected; completion callback is NULL");
        return false;
    }

    LOG_TRACE("nfcMgr: request technologies=%lu timeout=%lu keepOpen=%d",
              (unsigned long)cfg->technologies, (unsigned long)cfg->timeout,
              (int)cfg->keepOpenOnSuccess);

    if (!cfg->technologies) {
        LOG_TRACE("nfcMgr: run rejected; no technologies requested");
        return false;
    }

    if (cfg->technologies & ~supported) {
        LOG_TRACE("nfcMgr: run rejected; unsupported technology mask=%lu",
                  (unsigned long)(cfg->technologies & ~supported));
        return false;
    }

    if (active || entered || ownsReader) {
        LOG_TRACE("nfcMgr: run rejected; manager busy "
                  "active=%d entered=%d ownsReader=%d",
                  (int)active, (int)entered, (int)ownsReader);
        return false;
    }

    reader = picc();

    LOG_TRACE("nfcMgr: HAL reader lookup result=%p", (void*)reader);

    if (!reader) {
        LOG_TRACE("nfcMgr: run rejected; PICC reader is NULL");
        return false;
    }

    if (reader->opened) {
        LOG_TRACE("nfcMgr: run rejected; reader already open");
        return false;
    }

    /* Copy the configuration and borrow the callback context. */
    request      = *cfg;
    callback     = cb;
    callbackData = userData;

    scanId = 0;
    active = true;

    LOG_TRACE("nfcMgr: request accepted technologies=%lu timeout=%lu",
              (unsigned long)request.technologies,
              (unsigned long)request.timeout);

    LOG_TRACE("nfcMgr: setting NFC state timeout");

    OOP_CALL(manager.state, setTimeOut, request.timeout);

    LOG_TRACE("nfcMgr: requesting NFC state transition");

    SM_GOTO(manager.state);

    LOG_TRACE("nfcMgr: state transition requested successfully");

    return true;
}

/** Public cancellation shares the terminal cleanup path. */
static void cancel(void) {
    LOG_TRACE("nfcMgr: cancel requested active=%d entered=%d", (int)active,
              (int)entered);

    finish(NFC_RES_CANCELED, PICC_ERR_OK, NULL);
}

/**
 * Query whether a request is pending/active
 * (not whether its state has exited).
 */
static bool isBusy(void) {
    LOG_TRACE("nfcMgr: isBusy active=%d entered=%d", (int)active, (int)entered);

    return active;
}

/**
 * Explicit recovery after failed shutdown; do not retry while the state is
 * active.
 */
static PiccErr_t retryCleanup(void) {
    LOG_TRACE(
        "nfcMgr: retryCleanup requested active=%d entered=%d ownsReader=%d",
        (int)active, (int)entered, (int)ownsReader);

    if (active || entered) {
        LOG_TRACE("nfcMgr: retryCleanup rejected; manager is active");

        return PICC_ERR_OTHER;
    }

    PiccErr_t error = closeOwnedReader();

    LOG_TRACE("nfcMgr: retryCleanup complete error=%d ownsReader=%d",
              (int)error, (int)ownsReader);

    return error;
}

/* --------------------------------------------------------------------------
 * Manager initialization
 * -------------------------------------------------------------------------- */

/** Construct state and UI once, retaining the naming pattern used by inmgr().
 */
NfcMgr* nfcmgr(void) {
    LOG_TRACE("nfcMgr: nfcmgr requested initialized=%d", (int)initialized);

    if (initialized) {
        LOG_TRACE("nfcMgr: returning existing manager instance");
        return &manager;
    }

    manager.state = MEM_ALLOC(sizeof(State));

    LOG_TRACE("nfcMgr: state allocation result=%p", (void*)manager.state);

    if (!manager.state) {
        LOG_TRACE("nfcMgr: initialization failed; state allocation failed");
        return NULL;
    }

    OOP_CALL_CTOR(State, manager.state, NULL, "");

    manager.run          = run;
    manager.cancel       = cancel;
    manager.isBusy       = isBusy;
    manager.retryCleanup = retryCleanup;

    manager.state->vtable.enter         = STATE_ENTER(Nfc);
    manager.state->vtable.exit          = STATE_EXIT(Nfc);
    manager.state->vtable.handlePicc    = STATE_HANDLE(Nfc, PiccEvent);
    manager.state->vtable.handleKeypad  = STATE_HANDLE(Nfc, KeypadEvent);
    manager.state->vtable.handleTimeout = STATE_HANDLE(Nfc, TimeOutEvent);

    LOG_TRACE("nfcMgr: state callbacks registered");

    ui_create();

    initialized = true;

    LOG_TRACE("nfcMgr: initialization complete");

    return &manager;
}