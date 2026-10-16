#include "txnMgr.h"
#include "logger.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "inputMgr/inputMgr.h"
#include "ui/infoPage.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "infoMgr/infoMgr.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define RECOVERY_TXN_DATA_ADDR "/mtd0/txn_rec"
#define TAG_PENDING_TXN        0x0501

extern const TxnFlowConfig settlementTxnFlowCfg;
extern const TxnFlowConfig reverseTxnFlowCfg;

/* Snapshot configuration; caller may release its configuration structure. */
static TxnFlowConfig recoveryCfg;

extern uint64_t operationId;

/******************************************************************
 *                  Pending transaction persistence
 ******************************************************************/

/*
 * Recovery uses separate storage from the new transaction.
 *
 * __txnMgr.data and __txnMgr.flow belong to the new operation.
 * pendingData and recoveryFlow belong to the older operation.
 */
TxnData pendingData;
TxnFlow recoveryFlow;

/* True only while the manager is waiting for recovery completion. */
bool recoveryActive;

bool recoveryNotified;

static const DataDescriptor pendingTxnDsc[] = {
    DSC_BIN_ID(TAG_PENDING_TXN, pendingData)};

void complete(TxnMgrResult result, TxnMgrError err, TxnFlowStatus* status);

bool pendingRequired(const TxnData* data) {
    return data && (data->status == TXN_STATUS_PENDING_REVERSE ||
                    data->status == TXN_STATUS_PENDING_SETTLEMENT);
}

/*
 * Load the persisted record.
 *
 * Storage errors must not be interpreted as "nothing pending".
 * No caller may inspect pendingData after a failed load.
 */
Error_t pendingLoad(void) {
    LOG_TRACE("TxnMgr: loading pending record");
    TxnData previous = pendingData;
    Error_t err = storage()->load(pendingTxnDsc, ARRAY_COUNT(pendingTxnDsc),
                                  RECOVERY_TXN_DATA_ADDR);
    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: pendingLoad failed; storage code=%d", (int)err);
        /* A failed load may have partially changed descriptor destinations. */
        pendingData = previous;
    }
    /* TODO: Map only a documented NOT_FOUND result to first-boot creation.
     * Do not treat malformed/corrupt data or IO failure as an empty record.
     * Validate stored enum/status values using the complete TxnStatus enum.
     */
    LOG_TRACE("TxnMgr: pendingLoad finished; code=%d", (int)err);
    return err;
}

/*
 * Persist a complete record.
 *
 * Restore the in-memory record if saving fails.
 * Atomicity of the on-disk write remains the storage layer's
 * responsibility.
 */
Error_t pendingSave(const TxnData* data) {
    if (!data) {
        LOG_TRACE("TxnMgr: pendingSave rejected: null data");
        return ERR_NOK;
    }

    LOG_TRACE("TxnMgr: saving pending record; status=%d", (int)data->status);
    TxnData previous = pendingData;

    pendingData = *data;

    Error_t err = storage()->save(pendingTxnDsc, ARRAY_COUNT(pendingTxnDsc),
                                  RECOVERY_TXN_DATA_ADDR);

    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: pendingSave failed; storage code=%d", (int)err);
        pendingData = previous;
    }

    LOG_TRACE("TxnMgr: pendingSave finished; code=%d", (int)err);
    return err;
}

/*
 * Store a transaction with a new recovery status.
 *
 * Update the caller's status only after persistence succeeds.
 * Do not overwrite the record while recovery is in progress.
 */
Error_t pendingUpdate(TxnStatus status, TxnData* data) {
    if (!data || recoveryActive) {
        LOG_TRACE("TxnMgr: pendingUpdate rejected by argument/recovery guard");
        return ERR_NOK;
    }

    LOG_TRACE("TxnMgr: updating pending status=%d", (int)status);
    TxnData updated = *data;
    updated.status  = status;

    Error_t err = pendingSave(&updated);

    if (err == ERR_OK) {
        data->status = status;
    }

    LOG_TRACE("TxnMgr: pendingUpdate finished; code=%d", (int)err);
    return err;
}

/*
 * Record a new transaction requiring reversal if communication
 * does not reach the appropriate final outcome.
 *
 * Call only after the pending step has resolved the older record.
 */
Error_t pendingNew(TxnData* data) {
    if (!data || recoveryActive) {
        LOG_TRACE("TxnMgr: pendingNew rejected by argument/recovery guard");
        return ERR_NOK;
    }

    /* Never replace an unresolved older record, even if a caller skipped
     * TXN_STEP_PENDING or supplied an inconsistent flow configuration. */
    Error_t err = pendingLoad();
    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: pendingNew failed; storage code=%d", (int)err);
        return err;
    }
    if (pendingRequired(&pendingData)) {
        LOG_TRACE("TxnMgr: refusing to overwrite unresolved pending record");
        return ERR_NOK;
    }
    return pendingUpdate(TXN_STATUS_PENDING_REVERSE, data);
}

/*
 * Change only the status of the persisted record.
 *
 * Load first so a status change cannot accidentally save an
 * uninitialized or stale transaction after a restart.
 */
Error_t pendingMark(TxnStatus status) {
    if (recoveryActive) {
        LOG_TRACE("TxnMgr: pendingMark rejected: recovery active");
        return ERR_NOK;
    }

    Error_t err = pendingLoad();

    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: pendingMark failed; storage code=%d", (int)err);
        return err;
    }

    LOG_TRACE("TxnMgr: marking pending status=%d", (int)status);
    TxnData updated = pendingData;
    updated.status  = status;

    return pendingSave(&updated);
}

/*
 * Explicitly clear the recovery record.
 *
 * Use only when the application knows recovery is no longer needed.
 * Never reset merely because recovery communication failed.
 */
Error_t pendingReset(void) {
    LOG_TRACE("TxnMgr: pending reset requested");
    if (recoveryActive) {
        LOG_TRACE("TxnMgr: pendingReset rejected: recovery active");
        return ERR_NOK;
    }

    TxnData empty = {0};

    return pendingSave(&empty);
}

/*
 * Check persisted recovery state without starting communication.
 *
 * The boolean output is valid only when ERR_OK is returned.
 */
Error_t pendingCheck(bool* hasPending) {
    if (!hasPending) {
        LOG_TRACE("TxnMgr: pendingCheck rejected by argument/recovery guard");
        return ERR_NOK;
    }

    *hasPending = false;

    if (recoveryActive) {
        LOG_TRACE("TxnMgr: pendingCheck rejected by argument/recovery guard");
        return ERR_NOK;
    }

    Error_t err = pendingLoad();

    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: pendingCheck failed; storage code=%d", (int)err);
        return err;
    }

    *hasPending = pendingRequired(&pendingData);

    LOG_TRACE("TxnMgr: pending check complete; required=%d", (int)*hasPending);
    return ERR_OK;
}

/******************************************************************
 *                       Recovery completion
 ******************************************************************/

/*
 * Confirm that this callback belongs to the recovery flow while
 * the manager is waiting on the pending step.
 *
 * The flow must deliver one terminal callback and stop its timers
 * and transport work before another recovery operation can start.
 */
static bool isRecoveryCompletion(const TxnFlow* flow) {
    TxnMgr* mgr = txnMgr();

    return flow == &recoveryFlow && recoveryActive && mgr->active &&
           mgr->stepStarted && mgr->step == TXN_STEP_PENDING;
}

/*
 * Finish recovery of the old transaction.
 *
 * Success must be persisted before the new transaction advances.
 * On failure, leave the pending record available for a later retry.
 *
 * This preserves the existing success rule:
 * TXN_FLOW_SUCCESS together with RESP_CODE_SUCESS.
 */
static void recoveryDone(TxnFlow* flow, const TxnFlowStatus* status,
                         TxnStatus expectedStatus, TxnStatus completedStatus) {

    TxnMgr* mgr = txnMgr();

    if (!isRecoveryCompletion(flow)) {
        LOG_TRACE(
            "TxnMgr: ignoring recovery completion outside active recovery");
        return;
    }

    /*
     * The matching recovery callback must correspond to the
     * operation selected from the stored record.
     */
    LOG_TRACE(
        "TxnMgr: recovery completion; expected status=%d, target status=%d",
        (int)expectedStatus, (int)completedStatus);
    LOG_TRACE("TxnMgr: recovery flow result=%d, stage=%d, code=%d",
              status ? (int)status->result : -1,
              status ? (int)status->stage : -1, status ? status->code : -1);
    recoveryNotified = true;

    if (status->result != TXN_FLOW_SUCCESS) {
        recoveryActive = false;

        mgr->fail(status->result == TXN_FLOW_TIMEOUT
                      ? TXN_MGR_ERR_TIMEOUT
                      : TXN_MGR_ERR_PENDING_FLOW);
        return;
    }

    if (status->code != RESP_CODE_SUCESS) {
        recoveryActive = false;

        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_RESP_CODE, status);
        return;
    }

    /*
     * Preserve the original transaction fields.
     * Only its recovery status changes here.
     */
    TxnData completed = pendingData;
    completed.status  = completedStatus;

    Error_t err = pendingSave(&completed);

    recoveryActive = false;

    if (err != ERR_OK) {
        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_PENDING_SAVE, status);
        return;
    }

    /*
     * Recovery is complete and durably recorded.
     * Continue the new transaction's sequence.
     */
    LOG_TRACE("TxnMgr: recovery persisted; advancing new transaction");
    mgr->next();
}

void settleDone(TxnFlow* flow, const TxnFlowStatus* status) {
    recoveryDone(flow, status, TXN_STATUS_PENDING_SETTLEMENT,
                 TXN_STATUS_APPROVED);
}

void reverseDone(TxnFlow* flow, const TxnFlowStatus* status) {
    recoveryDone(flow, status, TXN_STATUS_PENDING_REVERSE, TXN_STATUS_REVERSED);
}

void checkPending(void) {
    LOG_TRACE("TxnMgr: pending recovery step entered");
    TxnMgr* mgr = txnMgr();

    if (!mgr->active || !mgr->stepStarted || mgr->step != TXN_STEP_PENDING ||
        recoveryActive) {
        LOG_TRACE("TxnMgr: pending step ignored: inactive, wrong step, or "
                  "recovery busy");
        return;
    }

    /*
     * Fail closed on storage errors.
     * An unreadable record may contain an unresolved transaction.
     */
    Error_t err = pendingLoad();

    if (err != ERR_OK) {
        mgr->fail(TXN_MGR_ERR_PENDING_LOAD);
        return;
    }

    const TxnFlowConfig* cfg = NULL;

    if (pendingData.status == TXN_STATUS_PENDING_REVERSE) {
        LOG_TRACE("TxnMgr: pending reversal selected");
        cfg = &reverseTxnFlowCfg;
    } else if (pendingData.status == TXN_STATUS_PENDING_SETTLEMENT) {
        LOG_TRACE("TxnMgr: pending settlement selected");
        cfg = &settlementTxnFlowCfg;
    } else {
        /* No recovery is required; continue immediately. */
        LOG_TRACE("TxnMgr: no recovery required");
        mgr->next();
        return;
    }

    /*
     * Prepare only the recovery flow.
     * Do not reset or overwrite mgr->data or mgr->flow.
     *
     * Preserve the existing NULL-owner convention. Confirm it
     * remains valid for your current TxnFlow implementation.
     */
    recoveryCfg            = *cfg;
    recoveryCfg.done       = pendingData.status == TXN_STATUS_PENDING_REVERSE
                                 ? reverseDone
                                 : settleDone;
    recoveryCfg.beforeSend = NULL;
    /* Original fields must not be replaced by new-transaction composition.
     * Recovery-specific build() must likewise preserve original identifiers. */
    recoveryCfg.compose = NULL;

    if (!txnFlowPrepare(&recoveryFlow, NULL, &recoveryCfg)) {
        LOG_TRACE("TxnMgr: recovery flow preparation failed");
        mgr->fail(TXN_MGR_ERR_PENDING_PREPARE);
        return;
    }

    /*
     * Restore original transaction fields after preparation.
     * Override only the outgoing recovery operation identifiers.
     */
    recoveryFlow.data = &pendingData;

    recoveryFlow.data->core.mti     = cfg->mti;
    recoveryFlow.data->core.txnType = cfg->type;

    /*
     * Static storage keeps the address valid if the flow retains
     * the pointer for asynchronous communication.
     */
    static char recoveryIp[32];

    normalizeIp(settings()->server.mainServerIp, recoveryIp,
                sizeof(recoveryIp));

    /*
     * Set the flag before connecting because completion may be
     * reported synchronously.
     */
    LOG_TRACE("TxnMgr: starting recovery connection");
    recoveryActive = true;

    recoveryNotified   = false;
    uint64_t startedId = operationId;
    bool     started   = txnFlowConnect(&recoveryFlow, recoveryIp,
                                        settings()->server.mainServerPort);

    LOG_TRACE("TxnMgr: recovery connect returned started=%d; operation=%llu",
              (int)started, (unsigned long long)startedId);

    /* Some false returns invoke done synchronously, others do not.
     * Never fail a new operation started from a completion callback. */
    if (!started && operationId == startedId && !recoveryNotified &&
        mgr->active && recoveryActive) {
        LOG_TRACE("TxnMgr: recovery start failed without completion callback");
        recoveryActive = false;
        mgr->fail(TXN_MGR_ERR_PENDING_FLOW);
    }
}
