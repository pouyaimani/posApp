#include "txnMgr.h"

#include "logger.h"
#include "infoMgr/infoMgr.h"
#include "phrases/phrases.h"
#include "states/states.h"
#include "ui/infoPage.h"

/******************************************************************
 *                      Communication messages
 ******************************************************************/

typedef struct {
    TxnFlowStage stage;
    Phrases_t    phrase;
} StageMessage;

static const StageMessage stageMessages[] = {
    {TXN_STAGE_CONNECTING, PHRASE_CONNECTION_ERR},
    {TXN_STAGE_SENDING, PHRASE_SENDING_DATA_ERR},
    {TXN_STAGE_RECEIVING, PHRASE_RECEIVING_DATA_ERR},
    {TXN_STAGE_PARSING, PHRASE_PARSE_ERROR}};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Choose a message from the flow status.
 * A timeout takes precedence over the communication stage.
 */
static Phrases_t communicationMessage(const TxnFlowStatus* st) {
    if (!st) {
        return PHRASE_CONNECTION_ERR;
    }

    if (st->result == TXN_FLOW_TIMEOUT) {
        return PHRASE_TIME_OUT;
    }

    for (size_t i = 0; i < ARRAY_COUNT(stageMessages); ++i) {
        if (stageMessages[i].stage == st->stage) {
            return stageMessages[i].phrase;
        }
    }

    return PHRASE_CONNECTION_ERR;
}

/******************************************************************
 *                         Result display
 ******************************************************************/

/*
 * Keep result arguments alive after the completion callback returns.
 *
 * TxnFlowStatus may originate from a local variable in TxnFlow's
 * completion function. The result screen must not retain that pointer.
 *
 * These snapshots remain valid until the next showTransactionResult().
 * TxnFlowConfig is a shallow copy: referenced field arrays and other
 * configuration targets must retain their normal application lifetime.
 */
static TxnFlowConfig resultCfg;
static TxnFlowStatus resultStatus;
static TxnData       resultData;

static void showFailure(const char* body, InfoDone cb) {
    memset(&infoFinishCtx, 0, sizeof(InfoFinishCtx));
    infoFinishCtx.done = cb;

    infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                   phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION),
                   body ? body : "");
}

static void digitalRecDone(InfoResult res, void* arg) {
    InfoDone done = (InfoDone)arg;
    LOG_TRACE("--------------digitalRecDone-----------------");
    if (done) {
        done(res, NULL);
    }
}

/*
 * Display approval or a host decline for the current transaction.
 * Return false if the information required by the result screen
 * is unavailable.
 */
static bool showTransactionResult(const TxnFlowStatus* st, const TxnData* data,
                                  InfoDone onDone, bool digitalRec) {
    TxnMgr* mgr = txnMgr();

    if (!st || !data || !mgr->config.txnFlowCfg) {
        LOG_TRACE("TxnResult: cannot show transaction result; "
                  "status=%d, data=%d, config=%d",
                  st != NULL, data != NULL, mgr->config.txnFlowCfg != NULL);

        return false;
    }

    resultData   = *data;
    resultStatus = *st;
    resultCfg    = *mgr->config.txnFlowCfg;

    memset(&infoFinishCtx, 0, sizeof(InfoFinishCtx));
    if (digitalRec) {
        infoFinishCtx.done     = digitalRecDone;
        infoFinishCtx.userData = onDone;
        infoMgr()->runDigitalReceipt(&infoFinishCtx, data);
        return true;
    }
    infoFinishCtx.done     = onDone;
    infoFinishCtx.userData = st;
    DEFINE_STRING(dsc, 128);
    if (st->code != 0) {
        getResponseCode(st->code, dsc, sizeof(dsc));
    }
    if (st->code == 0) {
        infoMgr()->run(INFO_SUCCESS, &infoFinishCtx,
                       phraseGetDef(PHRASE_SUCCESSFUL_TXN), "");
        return true;
    } else {
        infoMgr()->run(INFO_ERROR, &infoFinishCtx,
                       phraseGetDef(PHRASE_UNSUCCESSFUL_OPERATION), dsc);
        return true;
    }
    return true;
}

/******************************************************************
 *                         Start rejection
 ******************************************************************/

static void handleRejected(TxnMgrError err, InfoDone cb) {
    LOG_TRACE("TxnResult: transaction start rejected; error=%d", (int)err);

    /*
     * A rejected request may occur while another transaction is
     * running. Do not replace its screen or navigate it to idle.
     */
    if (txnMgr()->active) {
        LOG_TRACE("TxnResult: preserving active transaction after rejection");
        return;
    }

    /*
     * TODO:
     * Map individual rejection errors to dedicated phrases when
     * those phrases are available.
     */
    showFailure(phraseGetDef(PHRASE_TXN_REJECTED), cb);
}

/******************************************************************
 *                         Failed operation
 ******************************************************************/

static void handleFailure(TxnResultHandle handle) {
    LOG_TRACE("TxnResult: failure; step=%d, error=%d, flow=%d, code=%d",
              (int)handle.step, (int)handle.err,
              handle.flowStatus ? (int)handle.flowStatus->result : -1,
              handle.flowStatus ? handle.flowStatus->code : TXN_MGR_RESP_NONE);

    /*
     * Cancellation is a user action. Return to idle without an
     * unsuccessful-operation screen.
     */
    if (handle.err == TXN_MGR_ERR_CANCELED) {
        LOG_TRACE("TxnResult: transaction canceled");
        showFailure(phraseGetDef(PHRASE_TXN_CANCELLED), handle.onDone);
        return;
    }

    if (handle.err == TXN_MGR_ERR_TIMEOUT) {
        showFailure(phraseGetDef(PHRASE_TIME_OUT), handle.onDone);
        return;
    }

    if (handle.err == TXN_MGR_ERR_COMMUNICATION) {
        showFailure(phraseGetDef(communicationMessage(handle.flowStatus)),
                    handle.onDone);
        return;
    }

    if (handle.err == TXN_MGR_ERR_RESP_CODE) {
        /*
         * Recovery responses belong to the older transaction.
         * Do not show them using the new transaction's configuration
         * or its partially collected input data.
         */
        if (handle.step == TXN_STEP_PENDING) {
            LOG_TRACE("TxnResult: recovery host response rejected; code=%d",
                      handle.flowStatus ? handle.flowStatus->code
                                        : TXN_MGR_RESP_NONE);

            showFailure(phraseGetDef(PHRASE_PENDED_TXN_FAILURE), handle.onDone);
            return;
        }

        /*
         * A host response is meaningful only after successful parsing.
         * Missing or inconsistent result data falls back to an error.
         */
        if (handle.step == TXN_STEP_COMMUNICATION && handle.flowStatus &&
            handle.flowStatus->result == TXN_FLOW_SUCCESS &&
            handle.flowStatus->code != RESP_CODE_SUCESS &&
            showTransactionResult(handle.flowStatus, handle.data, handle.onDone,
                                  handle.digitalRec)) {
            return;
        }

        LOG_TRACE("TxnResult: invalid host-decline completion context");
        showFailure("", handle.onDone);
        return;
    }

    if (handle.step == TXN_STEP_PENDING) {
        /*
         * Covers recovery loading, preparation, persistence, and
         * processing failures without presenting a new-transaction
         * result.
         */
        showFailure(phraseGetDef(PHRASE_PENDED_TXN_FAILURE), handle.onDone);
        return;
    }

    if (handle.err == TXN_MGR_ERR_PENDING_SAVE && handle.flowStatus &&
        handle.flowStatus->result == TXN_FLOW_SUCCESS &&
        handle.flowStatus->code == RESP_CODE_SUCESS) {
        /*
         * The host approved, but local persistence failed.
         * Do not present an ordinary decline or encourage a retry.
         *
         * TODO:
         * Add a dedicated phrase explaining:
         * "Host approved; local processing failed. Check transaction
         * status before attempting another payment."
         */
        LOG_TRACE("TxnResult: host approved but local persistence failed");

        showFailure("", handle.onDone);
        return;
    }

    /*
     * TODO:
     * Add dedicated messages for invalid input, unsupported steps,
     * and other local errors as the phrase catalog is extended.
     */
    showFailure("", handle.onDone);
}

/******************************************************************
 *                         Public callback
 ******************************************************************/
void txnResultHandle(TxnResultHandle handle) {
    LOG_TRACE("TxnResult: completion; result=%d, step=%d, error=%d",
              (int)handle.result, (int)handle.step, (int)handle.err);

    if (handle.result == TXN_MGR_RESULT_REJECT) {
        handleRejected(handle.err, handle.onDone);
        return;
    }
    if (handle.result == TXN_MGR_RESULT_SUCCESS) {
        /*
         * Guard against contradictory completion information.
         * This manager's successful sequences end in communication.
         */
        if (handle.err != TXN_MGR_ERR_NONE ||
            (handle.step != TXN_STEP_COMMUNICATION &&
             handle.step != TXN_STEP_SETTLEMENT) ||
            !handle.data || !handle.flowStatus ||
            handle.flowStatus->result != TXN_FLOW_SUCCESS ||
            handle.flowStatus->code != RESP_CODE_SUCESS) {
            LOG_TRACE("TxnResult: inconsistent success notification");
            showFailure("", handle.onDone);
            return;
        }

        LOG_TRACE("TxnResult: displaying approved transaction");

        if (!showTransactionResult(handle.flowStatus, handle.data,
                                   handle.onDone, handle.digitalRec)) {
            showFailure("", handle.onDone);
        }

        return;
    }

    if (handle.result == TXN_MGR_RESULT_FAILURE &&
        handle.step == TXN_STEP_SETTLEMENT) {
        if (!showTransactionResult(handle.flowStatus, handle.data,
                                   handle.onDone, true)) {
            showFailure("", handle.onDone);
        }
        return;
    }

    if (handle.result == TXN_MGR_RESULT_FAILURE) {
        handleFailure(handle);
        return;
    }

    /* NONE and RUNNING are not terminal completion notifications. */
    LOG_TRACE("TxnResult: ignoring nonterminal result=%d", (int)handle.result);
}