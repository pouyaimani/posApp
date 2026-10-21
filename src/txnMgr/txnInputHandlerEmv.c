/**
 * Kahroba EMV integration for txnInputHandler.c.
 *
 * REPLACE your existing onNfcComplete(), nfcStartFailed(), getNfcData()
 * section with this file's code (or compile this file separately if those
 * functions are removed from txnInputHandler.c).
 *
 * Called on two serialized execution contexts:
 *  - Application / state-machine event loop: getNfcData(), onNfcComplete(),
 *    txnEmvOnCompletedEvent(), txnMgr->next()/fail().
 *  - Exclusive NFC APDU worker: txnEmvWorkerStep() only.
 *
 * The completion event queue must create a happens-before boundary between
 * worker and app thread. Never reset the EMV manager during an APDU exchange.
 */
#include "txnMgr.h"
#include "logger.h"
#include "nfcMgr/nfcMgr.h"
#include "emvMgr/emv_mgr.h"
#include "emvMgr/emv_kahroba.h"
#include "emvMgr/emv_picc_transport.h"
#include "emvMgr/emv_field55.h"
#include "emvMgr/emv_txn_bridge.h"
#include "txn_emv_port.h"

#include <stdint.h>
#include <string.h>

/** Persistent NFC configuration: run() may retain/copy its contents. */
static NfcCfg s_nfcCfg;

/** EMV module storage is static to avoid large objects on RTOS task stacks. */
static EmvMgr         s_emv;
static EmvTagDb       s_terminalTags;
static EmvPiccAdapter s_piccAdapter;
static EmvTxnBridge   s_bridge;
static bool           s_emvInitialized;

/**
 * Only the event-loop thread modifies this transaction lifetime record.
 * Worker accesses the EMV library state, not this record.
 */
static struct {
    TxnMgr*  txn;
    uint32_t txnId;
    uint32_t sessionId;
    int      stepAtStart;
    bool     active;
    bool     ownsReader;
} s_session;

/** Monotonic local request ID until TxnMgr exposes its own unique txn ID. */
static uint32_t s_nextTxnId;

/** Example host Field 55 tags: MUST be replaced with your Kahroba host spec. */
static const EmvField55Spec s_field55Spec[] = {
    {0x5F2Au, true}, {0x82u, true},    {0x95u, true},    {0x9Au, true},
    {0x9Cu, true},   {0x9F02u, true},  {0x9F03u, false}, {0x9F10u, false},
    {0x9F1Au, true}, {0x9F26u, true},  {0x9F27u, true},  {0x9F36u, true},
    {0x9F37u, true}, {0x9F34u, false}, {0x84u, false}};

/** Erase potentially sensitive Field 55 scratch bytes even in optimized builds.
 */
static void secureErase(void* data, size_t size) {
    volatile uint8_t* p = (volatile uint8_t*)data;
    while (size-- != 0u) {
        *p++ = 0u;
    }
}

/** Enqueue a copied completion event from the NFC worker. */
static bool postEmvEvent(void* context, const EmvCompletedEvent* event) {
    (void)context;
    return event != NULL && txnEmvPostCompletedEvent(event);
}

/** Construct EMV manager once; nfcMgr remains the PICC device owner. */
static bool ensureEmvInitialized(void) {
    EmvError error;
    if (s_emvInitialized) {
        return true;
    }
    s_piccAdapter.device = picc();
    if (s_piccAdapter.device == NULL) {
        LOG_TRACE("TxnEmv: PICC HAL unavailable");
        return false;
    }
    error = emvMgrInit(&s_emv, emvPiccTransport(&s_piccAdapter));
    if (error != EMV_OK) {
        LOG_TRACE("TxnEmv: emvMgrInit failed err=%d", (int)error);
        return false;
    }
    error = emvTxnBridgeAttach(&s_emv, &s_bridge, postEmvEvent, NULL);
    if (error != EMV_OK) {
        LOG_TRACE("TxnEmv: emvTxnBridgeAttach failed err=%d", (int)error);
        return false;
    }
    s_emvInitialized = true;
    return true;
}

/** Close only an explicitly retained PICC; nfcMgr owns other reader lifecycles.
 */
static bool releaseRetainedReader(void) {
    Picc*     device;
    PiccErr_t removeErr;
    PiccErr_t closeErr;
    if (!s_session.ownsReader) {
        return true;
    }
    device = picc();
    if (device == NULL) {
        return false;
    }
    /* For production, follow the Kahroba-specified card removal policy. */
    removeErr            = PICC_REMOVE(PICC_REMOVE_HALT);
    closeErr             = PICC_CLOSE();
    s_session.ownsReader = false;
    (void)removeErr; /* LOG_TRACE may be compiled out. */
    LOG_TRACE("TxnEmv: PICC released removeErr=%d closeErr=%d", (int)removeErr,
              (int)closeErr);
    /* A card might already be absent; close success is what matters here. */
    return closeErr == PICC_ERR_OK;
}

/** Tear down only after worker completion or before worker was scheduled. */
static bool resetSession(void) {
    bool released = releaseRetainedReader();
    if (s_emvInitialized) {
        emvMgrReset(&s_emv);
        emvTagDbClear(&s_terminalTags);
        s_bridge.post_failed = false;
    }
    memset(&s_session, 0, sizeof(s_session));
    return released;
}

/** True only while the original input step is still active. */
static bool originalTxnIsActive(void) {
    return s_session.active && s_session.txn != NULL &&
           s_session.txn == txnMgr() && s_session.txn->active &&
           s_session.txn->stepStarted &&
           (int)s_session.txn->step == s_session.stepAtStart;
}

/** Map reader and EMV errors without inventing txnMgr error enumeration names.
 */
static void failOriginalTxn(TxnMgr* txn, EmvOutcome outcome, EmvError error) {
    if (txn == NULL || !txn->active) {
        return;
    }
    LOG_TRACE("TxnEmv: failing transaction outcome=%d error=%d", (int)outcome,
              (int)error);
    if (outcome == EMV_OUTCOME_CANCELLED) {
        txn->fail(TXN_MGR_ERR_CANCELED);
    } else if (error == EMV_ERR_TIMEOUT) {
        txn->fail(TXN_MGR_ERR_TIMEOUT);
    } else {
        txnEmvFailTransaction(txn, outcome, error);
    }
}

/**
 * Called by nfcMgr on the APPLICATION event-loop thread.
 * Do NOT run blocking APDU processing inside this callback.
 */
static void onNfcComplete(const NfcStatus* status, void* userData) {
    TxnMgr*            txn      = (TxnMgr*)userData;
    EmvKahrobaTerminal terminal = {0};
    EmvStartParams     request  = {0};
    EmvError           error;

    if (txn != s_session.txn || !s_session.active) {
        LOG_TRACE("TxnEmv: obsolete NFC completion ignored");
        return;
    }
    if (status == NULL) {
        LOG_TRACE("TxnEmv: NFC status missing");
        goto failed;
    }
    s_session.ownsReader = status->readerRetained;
    LOG_TRACE("TxnEmv: NFC result=%d cardType=%d retained=%d cleanup=%d",
              (int)status->result, (int)status->card.cardType,
              (int)status->readerRetained, (int)status->cleanupError);

    if (!originalTxnIsActive()) {
        LOG_TRACE("TxnEmv: NFC completion belongs to obsolete txn step");
        (void)resetSession();
        return;
    }
    if (status->result == NFC_RES_CANCELED) {
        (void)resetSession();
        failOriginalTxn(txn, EMV_OUTCOME_CANCELLED, EMV_OK);
        return;
    }
    if (status->result == NFC_RES_TIMEOUT) {
        (void)resetSession();
        failOriginalTxn(txn, EMV_OUTCOME_FAILED, EMV_ERR_TIMEOUT);
        return;
    }
    if (status->result != NFC_RES_DETECTED ||
        status->cleanupError != PICC_ERR_OK || !status->readerRetained ||
        !status->readerOpen) {
        LOG_TRACE("TxnEmv: NFC did not provide an owned active reader");
        goto failed;
    }
    if (status->card.cardType != PICC_CARD_A &&
        status->card.cardType != PICC_CARD_B) {
        LOG_TRACE("TxnEmv: unsupported NFC card technology=%d",
                  (int)status->card.cardType);
        goto failed;
    }

    /* This is the HAL's scan generation ID, not the EMV APDU sequence. */
    s_session.sessionId = picc()->scanId;
    if (s_session.sessionId == 0u) {
        LOG_TRACE("TxnEmv: PICC scan ID was not started");
        goto failed;
    }
    /* Application must supply real terminal data and validated TVR/CVM. */
    if (!txnEmvBuildTerminal(txn, &terminal)) {
        LOG_TRACE("TxnEmv: terminal context not available");
        goto failed;
    }
    error = emvKahrobaPrepareTerminal(&s_terminalTags, &terminal, txnEmvRandom,
                                      NULL);
    if (error != EMV_OK) {
        LOG_TRACE("TxnEmv: prepare terminal failed err=%d", (int)error);
        goto failed;
    }

    request.transaction_id    = s_session.txnId;
    request.session_id        = s_session.sessionId;
    request.select_ppse_first = true;
    request.terminal_tags     = &s_terminalTags;

    s_bridge.post_failed = false;
    error                = emvMgrBegin(&s_emv, &request);
    /* emvMgrBegin copies its terminal tags; discard temporary sensitive data.
     */
    emvTagDbClear(&s_terminalTags);
    if (error != EMV_OK) {
        LOG_TRACE("TxnEmv: emvMgrBegin failed err=%d", (int)error);
        goto failed;
    }

    LOG_TRACE("TxnEmv: started txnId=%lu sessionId=%lu",
              (unsigned long)s_session.txnId,
              (unsigned long)s_session.sessionId);
    txnEmvScheduleWorker();
    return;

failed:
    (void)resetSession();
    failOriginalTxn(txn, EMV_OUTCOME_FAILED, EMV_ERR_PROTOCOL);
}

/**
 * Entry point from your TxnMgr NFC input step.
 * The retained reader is CLOSED only after the EMV completion is consumed.
 */
void getNfcData(void) {
    TxnMgr* txn = txnMgr();
    if (txn == NULL || !txn->active || !txn->stepStarted) {
        LOG_TRACE("TxnEmv: getNfcData outside active txn step");
        return;
    }
    if (s_session.active) {
        LOG_TRACE("TxnEmv: simultaneous NFC/EMV sessions forbidden");
        txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_STATE);
        return;
    }
    if (!ensureEmvInitialized()) {
        txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_STATE);
        return;
    }

    ++s_nextTxnId;
    if (s_nextTxnId == 0u) {
        ++s_nextTxnId;
    }
    memset(&s_session, 0, sizeof(s_session));
    s_session.txn         = txn;
    s_session.txnId       = s_nextTxnId;
    s_session.stepAtStart = (int)txn->step;
    s_session.active      = true;

    s_nfcCfg = (NfcCfg){.technologies      = PICC_TECH_A | PICC_TECH_B,
                        .keepOpenOnSuccess = true};
    if (!nfcmgr()->run(&s_nfcCfg, onNfcComplete, txn)) {
        LOG_TRACE("TxnEmv: nfcmgr()->run rejected");
        (void)resetSession();
        txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_STATE);
    }
}

/**
 * Called exclusively by the NFC worker after txnEmvScheduleWorker().
 * Never call emvMgrStep() from the GUI or a concurrent worker.
 */
void txnEmvWorkerStep(void) {
    EmvStepResult step = emvMgrStep(&s_emv);
    if (step == EMV_STEP_PROGRESS) {
        txnEmvScheduleWorker();
    } else if (step == EMV_STEP_REJECTED || s_bridge.post_failed) {
        LOG_TRACE("TxnEmv: worker could not post completion or invalid state");
        txnEmvNotifyWorkerFault();
    }
}

/**
 * Called on the txnMgr event-loop thread with a VALUE-COPIED event.
 * Caller must dispatch it from the application's normal event queue.
 */
void txnEmvOnCompletedEvent(const EmvCompletedEvent* event) {
    TxnMgr*  txn;
    EmvError error;
    uint8_t  field55[512];
    size_t   field55Len = 0u;
    bool     importOk   = false;
    bool     closeOk;

    if (event == NULL || !s_session.active ||
        event->transaction_id != s_session.txnId ||
        event->session_id != s_session.sessionId) {
        LOG_TRACE("TxnEmv: dropped stale completion event");
        return;
    }
    txn = s_session.txn;
    if (!originalTxnIsActive()) {
        LOG_TRACE("TxnEmv: txn advanced or stopped during NFC processing");
        (void)resetSession();
        return;
    }

    LOG_TRACE("TxnEmv: completed outcome=%d error=%d txnId=%lu sessionId=%lu",
              (int)event->outcome, (int)event->error,
              (unsigned long)event->transaction_id,
              (unsigned long)event->session_id);

    if (event->outcome == EMV_OUTCOME_ONLINE_REQUIRED &&
        event->error == EMV_OK) {
        const EmvResult* result = emvMgrResultView(&s_emv);
        error = emvTxnBridgeBuildField55(&s_emv, event, s_field55Spec,
                                         sizeof(s_field55Spec) /
                                             sizeof(s_field55Spec[0]),
                                         field55, sizeof(field55), &field55Len);
        if (error == EMV_OK && result != NULL) {
            /* Import must deep-copy everything before resetSession() erases. */
            importOk = txnEmvImportResult(txn, result, field55, field55Len);
        } else {
            LOG_TRACE("TxnEmv: Field 55 build failed err=%d", (int)error);
        }
    }
    secureErase(field55, sizeof(field55));
    closeOk = resetSession();
    if (!closeOk) {
        LOG_TRACE("TxnEmv: reader cleanup failed");
        txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_TRANSPORT);
        return;
    }
    if (event->outcome == EMV_OUTCOME_ONLINE_REQUIRED && importOk) {
        LOG_TRACE("TxnEmv: EMV data copied; advancing to authorization");
        txn->next();
    } else {
        failOriginalTxn(txn, event->outcome,
                        event->error != EMV_OK ? event->error
                                               : EMV_ERR_PROTOCOL);
    }
}
