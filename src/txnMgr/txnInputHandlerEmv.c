/*
 * Simplified Kahroba binding, intended to replace the earlier worker/event
 * bridge. Integrate into txnInputHandler.c after NFC detection.
 *
 * CRITICAL: onNfcComplete() must execute on a dedicated transaction task or
 * another thread permitted to block for the entire APDU sequence. If nfcMgr
 * calls back on the LVGL event loop, DO NOT execute this code there; schedule
 * a single synchronous transaction-task job instead.
 *
 * Application hooks txnEmvBuildTerminal/txnEmvRandom/txnEmvImportResult and
 * txnEmvFailTransaction were defined by the previous integration. The
 * supplied TxnMgr/NfcMgr types must be included from the real application.
 */
#include "txnMgr.h"
#include "nfcMgr/nfcMgr.h"
#include "emvMgr/emvMgr.h"
#include "emvMgr/emv_kahroba.h"
#include "emvMgr/emv_picc_transport.h"
#include "emvMgr/emv_field55.h"
#include "emvMgr/txn_emv_sync_port.h"
#include "logger.h"
#include <string.h>
#include "emvMgr/emv_kahroba.h"
#include "error.h"
#include "phrases/phrases.h"

static NfcCfg nfcCfg;

#if 0
/** Process the already-activated card with one blocking EMV call. */
static void onNfcComplete(const NfcStatus* status, void* userData) {
    TxnMgr*            txn      = (TxnMgr*)userData;
    EmvKahrobaTerminal terminal = {0};
    EmvStartParams     request  = {0};
    EmvOutcome         outcome  = EMV_OUTCOME_NONE;
    const EmvResult*   result;
    EmvError           error      = EMV_ERR_STATE;
    bool               owned      = status != NULL && status->readerRetained;
    size_t             field55Len = 0u;

    enum {
        ACT_NONE,
        ACT_NEXT,
        ACT_FAIL,
        ACT_CANCEL,
        ACT_TIMEOUT
    } action = ACT_FAIL;

    LOG_TRACE("NFC complete: statusPresent=%d txnPresent=%d readerOwned=%d",
              (int)(status != NULL), (int)(txn != NULL), (int)owned);

    if (status != NULL) {
        LOG_TRACE("NFC status: result=%d cardType=%d error=%d "
                  "cleanupError=%d readerRetained=%d readerOpen=%d",
                  (int)status->result, (int)status->card.cardType,
                  (int)status->error, (int)status->cleanupError,
                  (int)status->readerRetained, (int)status->readerOpen);
    }

    /* The actual app must also verify its active transaction/step identity. */
    if (txn == NULL || !txn->active || !txn->stepStarted) {
        LOG_TRACE(
            "NFC complete ignored: txnPresent=%d active=%d stepStarted=%d",
            (int)(txn != NULL), txn != NULL ? (int)txn->active : 0,
            txn != NULL ? (int)txn->stepStarted : 0);

        action = ACT_NONE;
        goto cleanup;
    }

    if (status == NULL) {
        LOG_TRACE("NFC failed: missing completion status");
        goto failed;
    }

    if (status->result == NFC_RES_CANCELED) {
        LOG_TRACE("NFC canceled: scheduling transaction cancellation");
        action = ACT_CANCEL;
        goto cleanup;
    }

    if (status->result == NFC_RES_TIMEOUT) {
        LOG_TRACE("NFC timeout: scheduling transaction timeout");
        action = ACT_TIMEOUT;
        goto cleanup;
    }

    if (status->result != NFC_RES_DETECTED || !status->readerRetained ||
        !status->readerOpen || status->cleanupError != PICC_ERR_OK ||
        (status->card.cardType != PICC_CARD_A &&
         status->card.cardType != PICC_CARD_B)) {
        LOG_TRACE("NFC failed: invalid card handoff "
                  "detected=%d retained=%d open=%d cleanupError=%d cardType=%d",
                  (int)(status->result == NFC_RES_DETECTED),
                  (int)status->readerRetained, (int)status->readerOpen,
                  (int)status->cleanupError, (int)status->card.cardType);
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: checking manager readiness");
    if (!ensureEmvReady()) {
        LOG_TRACE("Kahroba EMV failed: manager initialization/readiness");
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: building terminal configuration");
    if (!txnEmvBuildTerminal(txn, &terminal)) {
        LOG_TRACE("Kahroba EMV failed: terminal configuration build");
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: preparing terminal tags");
    error = emvKahrobaPrepareTerminal(&s_terminalTags, &terminal, txnEmvRandom,
                                      NULL);

    LOG_TRACE("Kahroba EMV: terminal preparation error=%d", (int)error);
    if (error != EMV_OK) {
        goto failed;
    }

    /* Nonzero local run identifier; no async result correlation is needed. */
    if (++s_generation == 0u) {
        ++s_generation;
        LOG_TRACE("Kahroba EMV: generation wrapped; skipped zero");
    }

    request.transaction_id    = s_generation;
    request.session_id        = s_generation;
    request.select_ppse_first = true;
    request.terminal_tags     = &s_terminalTags;

    LOG_TRACE(
        "Kahroba EMV: starting blocking run generation=%lu selectPpseFirst=%d",
        (unsigned long)s_generation, (int)request.select_ppse_first);

    error = emvMgrRun(&s_emv, &request, &outcome);

    LOG_TRACE("Kahroba EMV finished: error=%d outcome=%d", (int)error,
              (int)outcome);

    if (error != EMV_OK || outcome != EMV_OUTCOME_ONLINE_REQUIRED) {
        LOG_TRACE("Kahroba EMV failed: online-required outcome not obtained "
                  "error=%d outcome=%d",
                  (int)error, (int)outcome);
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: obtaining result view");
    result = emvMgrResultView(&s_emv);
    if (result == NULL) {
        LOG_TRACE("Kahroba EMV failed: result view is NULL");
        goto failed;
    }

    LOG_TRACE(
        "Kahroba EMV: building Field 55 requestedTagCount=%lu capacity=%lu",
        (unsigned long)(sizeof(s_hostTags) / sizeof(s_hostTags[0])),
        (unsigned long)sizeof(s_field55));

    error = emvField55Build(&result->tags, s_hostTags,
                            sizeof(s_hostTags) / sizeof(s_hostTags[0]),
                            s_field55, sizeof(s_field55), &field55Len);

    LOG_TRACE("Kahroba EMV: Field 55 build error=%d length=%lu", (int)error,
              (unsigned long)field55Len);

    if (error != EMV_OK) {
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: importing result into transaction");
    if (!txnEmvImportResult(txn, result, s_field55, field55Len)) {
        LOG_TRACE("Kahroba EMV failed: transaction result import");
        goto failed;
    }

    LOG_TRACE("Kahroba EMV: result imported; scheduling next transaction step");
    action = ACT_NEXT;
    goto cleanup;

failed:
    LOG_TRACE("Kahroba EMV: failure path error=%d outcome=%d", (int)error,
              (int)outcome);
    action = ACT_FAIL;

cleanup:
    LOG_TRACE(
        "Kahroba EMV cleanup: begin action=%d initialized=%d readerOwned=%d",
        (int)action, (int)s_initialized, (int)owned);

    /* Result has been copied into TxnData before reset. */
    LOG_TRACE("Kahroba EMV cleanup: wiping Field 55 buffer");
    wipeField55();

    if (s_initialized) {
        LOG_TRACE("Kahroba EMV cleanup: resetting EMV manager");
        emvMgrReset(&s_emv);
    } else {
        LOG_TRACE(
            "Kahroba EMV cleanup: reset skipped; manager not initialized");
    }

    LOG_TRACE("Kahroba EMV cleanup: clearing terminal tags");
    emvTagDbClear(&s_terminalTags);

    if (owned) {
        PiccErr_t removeError;
        PiccErr_t closeError;

        LOG_TRACE("Kahroba EMV cleanup: removing card with HALT");
        removeError = PICC_REMOVE(PICC_REMOVE_HALT);
        LOG_TRACE("Kahroba EMV cleanup: PICC remove error=%d",
                  (int)removeError);

        /* Attempt close even if card removal failed. */
        LOG_TRACE("Kahroba EMV cleanup: closing PICC reader");
        closeError = PICC_CLOSE();
        LOG_TRACE("Kahroba EMV cleanup: PICC close error=%d", (int)closeError);
    } else {
        LOG_TRACE(
            "Kahroba EMV cleanup: PICC release skipped; reader not owned");
    }

    LOG_TRACE("Kahroba EMV cleanup: finished");

    /* Navigate ONLY after attempting hardware release and erasing session data.
     */
    if (txn != NULL && txn->active) {
        switch (action) {
        case ACT_NEXT:
            LOG_TRACE("Kahroba EMV action: advance transaction");
            txn->next();
            break;

        case ACT_CANCEL:
            LOG_TRACE("Kahroba EMV action: fail transaction as canceled");
            txn->fail(TXN_MGR_ERR_CANCELED);
            break;

        case ACT_TIMEOUT:
            LOG_TRACE("Kahroba EMV action: fail transaction as timed out");
            txn->fail(TXN_MGR_ERR_TIMEOUT);
            break;

        case ACT_FAIL:
            LOG_TRACE(
                "Kahroba EMV action: fail transaction outcome=%d error=%d",
                (int)outcome, (int)error);
            txnEmvFailTransaction(txn, outcome, error);
            break;

        case ACT_NONE:
        default:
            LOG_TRACE("Kahroba EMV action: no transaction navigation");
            break;
        }
    } else {
        LOG_TRACE(
            "Kahroba EMV action skipped: txnPresent=%d active=%d action=%d",
            (int)(txn != NULL), txn != NULL ? (int)txn->active : 0,
            (int)action);
    }
}
#endif

void nfcInfoDone(InfoResult res, void* arg) {
    txnMgr()->fail(TXN_MGR_ERR_INPUT);
}

static void onNfcComplete(const NfcStatus* status, void* userData) {
    RETURN_IF_NULL(status, ;);
    TxnMgr* txnMgr = userData;

    if (status->result != NFC_RES_DETECTED) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_DETECT_ERROR), "");
        LOG_TRACE("NFC detection error. result = %d", status->result);
        return;
    }

    if (!status->readerRetained) {
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_ERROR), "");
        LOG_TRACE("Kahroba: NFC reader was not retained");
        return;
    }

    switch (status->card.cardType) {
    case PICC_CARD_A:
    case PICC_CARD_B:
        LOG_TRACE("Kahroba: compatible card candidate detected");
        {
            EmvMgrError err = startKahrobaEmv(picc(), &txnMgr->data);
            LOG_TRACE("Kahroba: EmvMgrError = %d", err);
            (void)OOP_CALL(picc(), close);
        }
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_CARD_TYPE_ERROR), "");
        break;

    default:
        LOG_TRACE("Kahroba: unsupported card type=%d",
                  (int)status->card.cardType);

        /* Caller owns the reader and must release it. */
        (void)OOP_CALL(picc(), close);
        infoMgr()->run(INFO_ERROR, &(InfoFinishCtx){.done = nfcInfoDone},
                       phraseGetDef(PHRASE_NFC_CARD_TYPE_ERROR), "");
        return;
    }
}

/** Begins the existing nfcMgr detection; reader retained for EMV APDUs. */
void getNfcData(void) {
    nfcCfg = (NfcCfg){.technologies      = PICC_TECH_A | PICC_TECH_B,
                      .keepOpenOnSuccess = true};
    if (!nfcmgr()->run(&nfcCfg, onNfcComplete, txnMgr())) {
        TxnMgr* txn = txnMgr();
        if (txn != NULL && txn->active) {
            // txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_STATE);
        }
    }
}
