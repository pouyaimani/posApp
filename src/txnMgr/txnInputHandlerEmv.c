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

/* Persistent objects: avoid placing large EMV buffers on a small RTOS stack. */
static EmvMgr         s_emv;
static EmvTagDb       s_terminalTags;
static EmvPiccAdapter s_readerAdapter;
static NfcCfg         s_nfcCfg;
static bool           s_initialized;
static uint32_t       s_generation;
static uint8_t        s_field55[512];

/* EXAMPLE ONLY: replace the list with your Kahroba acquirer's requirements. */
static const EmvField55Spec s_hostTags[] = {
    {0x5F2Au, true}, {0x82u, true},    {0x95u, true},    {0x9Au, true},
    {0x9Cu, true},   {0x9F02u, true},  {0x9F03u, false}, {0x9F10u, false},
    {0x9F1Au, true}, {0x9F26u, true},  {0x9F27u, true},  {0x9F36u, true},
    {0x9F37u, true}, {0x9F34u, false}, {0x84u, false}};

static bool ensureEmvReady(void) {
    if (s_initialized) {
        return true;
    }
    s_readerAdapter.device = picc();
    if (s_readerAdapter.device == NULL) {
        return false;
    }
    if (emvMgrInit(&s_emv, emvPiccTransport(&s_readerAdapter)) != EMV_OK) {
        return false;
    }
    /* Deliberately DO NOT register an EMV event callback. */
    s_initialized = true;
    return true;
}

static void wipeField55(void) {
    size_t            i;
    volatile uint8_t* p = s_field55;
    for (i = 0u; i < sizeof(s_field55); ++i) {
        p[i] = 0u;
    }
}

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

    /* The actual app must also verify its active transaction/step identity. */
    if (txn == NULL || !txn->active || !txn->stepStarted) {
        action = ACT_NONE;
        goto cleanup;
    }
    if (status == NULL) {
        goto failed;
    }
    if (status->result == NFC_RES_CANCELED) {
        action = ACT_CANCEL;
        goto cleanup;
    }
    if (status->result == NFC_RES_TIMEOUT) {
        action = ACT_TIMEOUT;
        goto cleanup;
    }
    if (status->result != NFC_RES_DETECTED || !status->readerRetained ||
        !status->readerOpen || status->cleanupError != PICC_ERR_OK ||
        (status->card.cardType != PICC_CARD_A &&
         status->card.cardType != PICC_CARD_B)) {
        goto failed;
    }
    if (!ensureEmvReady() || !txnEmvBuildTerminal(txn, &terminal)) {
        goto failed;
    }
    error = emvKahrobaPrepareTerminal(&s_terminalTags, &terminal, txnEmvRandom,
                                      NULL);
    if (error != EMV_OK) {
        goto failed;
    }

    /* Nonzero local run identifier; no async result correlation is needed. */
    if (++s_generation == 0u) {
        ++s_generation;
    }
    request.transaction_id    = s_generation;
    request.session_id        = s_generation;
    request.select_ppse_first = true;
    request.terminal_tags     = &s_terminalTags;

    error = emvMgrRun(&s_emv, &request, &outcome);
    LOG_TRACE("Kahroba EMV finished: error=%d outcome=%d", (int)error,
              (int)outcome);
    if (error != EMV_OK || outcome != EMV_OUTCOME_ONLINE_REQUIRED) {
        goto failed;
    }

    result = emvMgrResultView(&s_emv);
    if (result == NULL) {
        goto failed;
    }
    error = emvField55Build(&result->tags, s_hostTags,
                            sizeof(s_hostTags) / sizeof(s_hostTags[0]),
                            s_field55, sizeof(s_field55), &field55Len);
    if (error != EMV_OK) {
        goto failed;
    }
    if (!txnEmvImportResult(txn, result, s_field55, field55Len)) {
        goto failed;
    }
    action = ACT_NEXT;
    goto cleanup;

failed:
    action = ACT_FAIL;
cleanup:
    /* Result has been copied into TxnData before reset. */
    wipeField55();
    if (s_initialized) {
        emvMgrReset(&s_emv);
    }
    emvTagDbClear(&s_terminalTags);
    if (owned) {
        (void)PICC_REMOVE(PICC_REMOVE_HALT);
        (void)PICC_CLOSE();
    }
    /* Navigate ONLY after releasing hardware and erasing session data. */
    if (txn != NULL && txn->active) {
        switch (action) {
        case ACT_NEXT:
            txn->next();
            break;
        case ACT_CANCEL:
            txn->fail(TXN_MGR_ERR_CANCELED);
            break;
        case ACT_TIMEOUT:
            txn->fail(TXN_MGR_ERR_TIMEOUT);
            break;
        case ACT_FAIL:
            txnEmvFailTransaction(txn, outcome, error);
            break;
        case ACT_NONE:
        default:
            break;
        }
    }
}

/** Begins the existing nfcMgr detection; reader retained for EMV APDUs. */
void getNfcData(void) {
    s_nfcCfg = (NfcCfg){.technologies      = PICC_TECH_A | PICC_TECH_B,
                        .keepOpenOnSuccess = true};
    if (!nfcmgr()->run(&s_nfcCfg, onNfcComplete, txnMgr())) {
        TxnMgr* txn = txnMgr();
        if (txn != NULL && txn->active) {
            txnEmvFailTransaction(txn, EMV_OUTCOME_FAILED, EMV_ERR_STATE);
        }
    }
}
