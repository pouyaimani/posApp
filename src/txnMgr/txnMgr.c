/*
 * Reviewed TxnMgr implementation.
 * Requires txnFlow.h with beforeSend and the revised TxnFlow that performs
 * no pending persistence itself. Project headers/build are not included here.
 *
 * Remaining integration gates:
 * - InputMgr must commit output before completion, clear a previous commit
 *   target when starting PIN, stop old timers/providers, and reject stale
 * events.
 * - SM_GOTO same-State restart and transition scheduling need confirmation.
 * - Storage must define NOT_FOUND, record validation and atomic durable writes.
 * - Host-decline recovery policy and response validation require protocol
 * rules.
 * - Config-specific side effects (for example settings()->save()) belong in
 *   the manager completion callbacks; original flow .done is intentionally
 * replaced.
 * - fail() reports a result; it does not implement transport/input
 * cancellation.
 * - Implement unfinished step templates before enabling their table entries.
 */
#include "txnMgr.h"
#include "logger.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "common.h"
#include "inputMgr/inputMgr.h"
#include "ui/infoPage.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "infoMgr/infoMgr.h"
#include "txnDef.h"

/* True only while the manager is waiting for recovery completion. */
extern bool recoveryActive;

static TxnMgr  __txnMgr;
static State   __txnMgrState;
static TxnFlow __flow;

/* Snapshot configuration; caller may release its configuration structure. */
static TxnFlowConfig operationCfg;

/* Guard fallback handling after a synchronous completion starts another run. */
uint64_t    operationId;
static bool communicationNotified;
extern bool recoveryNotified;

/*
 * Recovery uses separate storage from the new transaction.
 *
 * __txnMgr.data and __txnMgr.flow belong to the new operation.
 * pendingData and recoveryFlow belong to the older operation.
 */
extern TxnData pendingData;
extern TxnFlow recoveryFlow;

extern const TxnFlowConfig settlementTxnFlowCfg;

/*
 * Settlement has its own flow and working data.
 *
 * The main transaction data remains available for the result screen.
 * In particular, do not overwrite its MTI, transaction type, or
 * original-data snapshot with settlement request values.
 */
static TxnFlow       settlementFlow;
static TxnFlowConfig settlementConfig;
static TxnData       settlementData;

static bool     settlementActive;
static uint64_t settlementOperationId;

/* All manager entry points and provider callbacks must be serialized.
 * Provider cancellation and stale-event filtering remain provider contracts.
 * See TODO comments before enabling the unimplemented transaction steps.
 */

/******************************************************************
 *                      Forward declarations
 ******************************************************************/

static void executeStep(void);
void        settleDone(TxnFlow* flow, const TxnFlowStatus* status);
void        reverseDone(TxnFlow* flow, const TxnFlowStatus* status);

void enterAmount(void);
void enterBillId(void);
void enterPaymentId(void);
void selectOperator(void);
void enterChargeAmount(void);
void enterPhone(void);
void enterPin(void);
void checkPending(void);
void communication(void);
void getNfcData(void);
void captureMagData();

void onInputComplete(InputResult result, void* userData);
void onInputTimeout(InputResult result, void* userData);

void complete(TxnMgrResult result, TxnMgrError err, TxnFlowStatus* status);

Error_t pendingLoad(void);

void checkPending(void);

/*
 * Persist a complete record.
 *
 * Restore the in-memory record if saving fails.
 * Atomicity of the on-disk write remains the storage layer's
 * responsibility.
 */
Error_t pendingSave(const TxnData* data);

/*
 * Store a transaction with a new recovery status.
 *
 * Update the caller's status only after persistence succeeds.
 * Do not overwrite the record while recovery is in progress.
 */
Error_t pendingUpdate(TxnStatus status, TxnData* data);

/*
 * Record a new transaction requiring reversal if communication
 * does not reach the appropriate final outcome.
 *
 * Call only after the pending step has resolved the older record.
 */
Error_t pendingNew(TxnData* data);

/*
 * Change only the status of the persisted record.
 *
 * Load first so a status change cannot accidentally save an
 * uninitialized or stale transaction after a restart.
 */
Error_t pendingMark(TxnStatus status);
/*
 * Explicitly clear the recovery record.
 *
 * Use only when the application knows recovery is no longer needed.
 * Never reset merely because recovery communication failed.
 */
Error_t pendingReset(void);

/*
 * Check persisted recovery state without starting communication.
 *
 * The boolean output is valid only when ERR_OK is returned.
 */
Error_t pendingCheck(bool* hasPending);

/**
 * @brief Resolve host responses for Kahroba purchase transactions.
 *
 * Rules:
 * - Approved response: continue the transaction.
 * - Response 76 on first authorization: request online PIN.
 * - Response 76 on subsequent authorization: decline.
 * - Other responses: decline.
 *
 * This callback only selects an action.
 * It must not change transaction state or perform I/O.
 */
TxnHostAction kahrobaResponsePolicy(TxnMgrStep step, int responseCode,
                                    uint8_t attempt);

/**
 * @brief Request PIN only if required by the host.
 */
static void enterHostPin(void);
/**
 * @brief Execute PIN authorization only when requested.
 */
static void communicationWithPin(void);

/**
 * @brief Determine whether the current step is an authorization
 *        communication step.
 */
static bool isCommunicationStep(const TxnMgr* mgr) {
    return mgr && mgr->active && mgr->stepStarted &&
           (mgr->step == TXN_STEP_COMMUNICATION ||
            mgr->step == TXN_STEP_COMMUNICATION_PIN);
}

/******************************************************************
 *                         Pending step
 ******************************************************************/

/*
 * Tracks whether this operation successfully created its own
 * pending record. Reset before starting new communication.
 */
static bool currentPendingSaved;

/*
 * Persist the current request before transmission.
 *
 * This preserves the existing policy that needSettlement also
 * identifies transactions requiring reversal protection.
 */
static int beforeCommunicationSend(TxnFlow* flow) {
    TxnMgr* mgr = &__txnMgr;

    if (!flow || flow != mgr->flow || !flow->cfg || !mgr->active ||
        !isCommunicationStep(mgr) || !mgr->stepStarted) {
        LOG_TRACE("TxnMgr: before-send rejected: invalid or inactive flow");
        return ERR_NOK;
    }

    if (!flow->cfg->needSettlement) {
        LOG_TRACE("TxnMgr: before-send: recovery persistence not required");
        return ERR_OK;
    }

    LOG_TRACE("TxnMgr: saving reversal record before transmission");
    Error_t err = mgr->pendingNew(flow->data);

    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: transmission blocked: pending save code=%d",
                  (int)err);
        return TXN_MGR_ERR_PENDING_SAVE;
    }

    currentPendingSaved = true;
    LOG_TRACE("TxnMgr: reversal record saved; transmission allowed");

    return ERR_OK;
}

/**
 * @brief Finish a transaction after a valid host decline.
 *
 * The pending reversal record may be removed only when
 * the host response is known to be a definitive non-approval.
 */
static void handleHostDecline(TxnMgr* mgr, const TxnFlowStatus* status) {
    if (!mgr || !status)
        return;

    LOG_TRACE("TxnMgr: host declined response=%d", status->code);

    if (currentPendingSaved) {
        Error_t err = mgr->pendingReset();

        if (err != ERR_OK) {
            LOG_TRACE("TxnMgr: pending reset failed error=%d", (int)err);

            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }

        currentPendingSaved = false;
    }

    complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_RESP_CODE,
             (TxnFlowStatus*)status);
}

/**
 * @brief Handle a host request for online PIN verification.
 *
 * Only the first authorization may request a PIN challenge.
 *
 * PIN entry and repeat authorization are handled by subsequent
 * transaction steps, not by this function.
 *
 * @note The current step sequence must contain TXN_STEP_PIN
 *       immediately after TXN_STEP_COMMUNICATION.
 */
static void handleHostPinRequired(TxnMgr* mgr, const TxnFlowStatus* status) {
    if (!mgr || !status)
        return;

    LOG_TRACE("TxnMgr: host PIN required operation=%llu attempt=%u",
              (unsigned long long)operationId,
              (unsigned)mgr->authorizationAttempt);

    /*
     * Do not proceed until the PIN step is installed.
     * For now preserve the previous decline behavior.
     */
    if (!mgr->steps || mgr->stepIndex + 1u >= mgr->stepCount ||
        mgr->steps[mgr->stepIndex + 1u] != TXN_STEP_HOST_PIN) {

        LOG_TRACE("TxnMgr: host PIN step not configured");

        handleHostDecline(mgr, status);
        return;
    }

    /*
     * The first authorization may already have a pending
     * reversal record.
     *
     * Code 76 must be confirmed as a definitive non-approval
     * before it is safe to clear that record.
     *
     * Do not enable the transition below for live transactions
     * until this host rule is confirmed.
     */
    if (currentPendingSaved) {
        Error_t err = mgr->pendingReset();

        if (err != ERR_OK) {
            LOG_TRACE("TxnMgr: cannot continue PIN challenge; "
                      "pending reset failed=%d",
                      (int)err);

            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }

        currentPendingSaved = false;
    }

    mgr->hostPinRequired = true;

    LOG_TRACE("TxnMgr: advancing to host PIN step");

    if (!mgr->next()) {
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
    }
}

/**
 * @brief Resolve a successful transport exchange's host response.
 *
 * If the transaction provides a custom policy, use it.
 * Otherwise, preserve the existing approval/decline behavior.
 *
 * @note This function performs no transaction side effects.
 */
static TxnHostAction resolveHostResponse(const TxnMgr* mgr, int responseCode) {
    if (!mgr)
        return TXN_HOST_DECLINE;

    if (mgr->config.responsePolicy) {
        return mgr->config.responsePolicy(mgr->step, responseCode,
                                          mgr->authorizationAttempt);
    }

    return responseCode == RESP_CODE_SUCESS ? TXN_HOST_APPROVE
                                            : TXN_HOST_DECLINE;
}

/*
 * Complete the current transaction after TxnFlow releases its
 * communication resources.
 */
static void mainTxnFlowDone(TxnFlow* flow, const TxnFlowStatus* status) {
    LOG_TRACE("Txn Mgr: mainTxnFlowDone ...");

    TxnMgr* mgr = &__txnMgr;

    if (!flow || flow != mgr->flow || !mgr->active || !mgr->stepStarted ||
        !isCommunicationStep(mgr)) {
        LOG_TRACE(
            "TxnMgr: ignoring communication completion outside active step");
        return;
    }

    LOG_TRACE("Txn Mgr: respcode = %d", flow->data->core.respCode);

    communicationNotified = true;
    if (!status) {
        LOG_TRACE("TxnMgr: communication completion has no status");
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
        return;
    }
    LOG_TRACE("TxnMgr: communication result=%d, stage=%d, code=%d",
              (int)status->result, (int)status->stage, status->code);

    mgr->flowStatus = *status;

    if (status->result != TXN_FLOW_SUCCESS) {
        /* No valid host response: code may be a transport/parser error. */
        mgr->fail(status->result == TXN_FLOW_TIMEOUT
                      ? TXN_MGR_ERR_TIMEOUT
                      : TXN_MGR_ERR_COMMUNICATION);
        return;
    }

    TxnHostAction action = resolveHostResponse(mgr, status->code);

    LOG_TRACE("TxnMgr: response=%d action=%d attempt=%u", status->code,
              (int)action, (unsigned)mgr->authorizationAttempt);

    switch (action) {

    case TXN_HOST_APPROVE:
        /* Continue to the existing approval logic below. */
        break;

    case TXN_HOST_REQUIRE_PIN:
        handleHostPinRequired(mgr, status);
        return;

    case TXN_HOST_DECLINE:
    default:
        handleHostDecline(mgr, status);
        return;
    }

    mgr->data.status = TXN_STATUS_APPROVED;

    if (currentPendingSaved) {
        LOG_TRACE("TxnMgr: host approved; persisting settlement record");
        /*
         * Persist the approved response, including host identifiers,
         * before reporting success to the application.
         */
        Error_t err =
            mgr->pendingUpdate(TXN_STATUS_PENDING_SETTLEMENT, &mgr->data);

        if (err != ERR_OK) {
            /*
             * The host approved the transaction, but local recovery
             * persistence failed. Preserve the response in mgr->data
             * and report the local error.
             */
            complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_PENDING_SAVE,
                     status); /* Host approved; local persistence failed. */
            return;
        }

        mgr->data.status = TXN_STATUS_PENDING_SETTLEMENT;
    }

    LOG_TRACE("TxnMgr: main communication completed successfully");
    mgr->next();
}

/******************************************************************
 *                      Settlement steps
 ******************************************************************/

/*
 * Return true only while the manager is waiting for this settlement.
 *
 * Transport callbacks must be stopped by txnFlowRelease() before
 * settlementFlow can be reused for another operation.
 */
static bool isSettlementStep(const TxnMgr* mgr) {
    return mgr && mgr->active && mgr->stepStarted &&
           mgr->step == TXN_STEP_SETTLEMENT;
}

/*
 * TxnFlow calls this after releasing its transport resources.
 * Copy the status because the callback argument may be stack-owned.
 */
static void onSettlementDone(TxnFlow* flow, const TxnFlowStatus* status) {
    TxnMgr* mgr = &__txnMgr;

    if (flow != &settlementFlow || !settlementActive ||
        settlementOperationId != operationId || !isSettlementStep(mgr)) {
        LOG_TRACE("TxnMgr: ignoring unexpected settlement completion");
        if (mgr->pendingUpdate(TXN_STATUS_PENDING_REVERSE, &mgr->data) !=
            ERR_OK) {
            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_FLOW);
        return;
    }

    settlementActive = false;

    if (!status) {
        LOG_TRACE("TxnMgr: settlement completed without status");
        if (mgr->pendingUpdate(TXN_STATUS_PENDING_REVERSE, &mgr->data) !=
            ERR_OK) {
            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_FLOW);
        return;
    }

    mgr->flowStatus = *status;

    LOG_TRACE("TxnMgr: settlement completed; result=%d, stage=%d, code=%d",
              (int)status->result, (int)status->stage, status->code);

    if (status->result != TXN_FLOW_SUCCESS) {
        if (mgr->pendingUpdate(TXN_STATUS_PENDING_REVERSE, &mgr->data) !=
            ERR_OK) {
            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }
        /*
         * Preserve the pending settlement for later recovery.
         * A timeout does not establish whether the host settled it.
         */
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_FLOW);
        return;
    }

    if (status->code != RESP_CODE_SUCESS) {
        LOG_TRACE("TxnMgr: settlement rejected by host; code=%d", status->code);
        if (mgr->pendingUpdate(TXN_STATUS_PENDING_REVERSE, &mgr->data) !=
            ERR_OK) {
            mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
            return;
        }

        /* Keep the original pending settlement record. */
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_FLOW);
        return;
    }

    /*
     * Persist completion using MAIN transaction data.
     *
     * Do not save settlementData here: its MTI and transaction type
     * describe the settlement request, not the original payment.
     *
     * Use a copy because pendingUpdate() may modify its argument
     * even when the storage write fails.
     */
    TxnData approvedData = mgr->data;

    Error_t err = mgr->pendingUpdate(TXN_STATUS_APPROVED, &approvedData);

    if (err != ERR_OK) {
        LOG_TRACE("TxnMgr: settlement approved but saving completion "
                  "failed; error=%d",
                  (int)err);

        mgr->fail(TXN_MGR_ERR_PENDING_SAVE);
        return;
    }

    mgr->data.status = TXN_STATUS_APPROVED;

    LOG_TRACE("TxnMgr: settlement approved and completion saved");

    /*
     * With settlement as the last step, next() completes the manager.
     * No success notification is emitted before this point.
     */
    mgr->next();
}

static void settleCurrentTransaction(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!isSettlementStep(mgr)) {
        LOG_TRACE("TxnMgr: settlement ignored outside settlement step");
        return;
    }

    if (!mgr->config.txnFlowCfg) {
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_CONTEXT);
        return;
    }

    if (!mgr->config.txnFlowCfg->needSettlement) {
        LOG_TRACE("TxnMgr: settlement not required");
        mgr->next();
        return;
    }

    /*
     * Communication must have received approval and durably saved
     * this transaction as pending settlement before advancing here.
     *
     * currentPendingSaved alone is insufficient: it may refer to
     * the pending reversal saved before the original send.
     */
    if (mgr->flowStatus.result != TXN_FLOW_SUCCESS ||
        mgr->flowStatus.code != RESP_CODE_SUCESS || !currentPendingSaved ||
        mgr->data.status != TXN_STATUS_PENDING_SETTLEMENT) {
        LOG_TRACE("TxnMgr: settlement rejected; "
                  "approved pending-settlement context missing");

        mgr->fail(TXN_MGR_ERR_SETTLEMENT_CONTEXT);
        return;
    }

    if (settlementActive || settlementFlow.tx ||
        settlementFlow.stage != TXN_STAGE_IDLE) {
        LOG_TRACE("TxnMgr: settlement flow is busy");
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_BUSY);
        return;
    }

    txnFlowInit(&settlementFlow);
    settlementActive      = false;
    settlementOperationId = 0;

    settlementConfig      = settlementTxnFlowCfg;
    settlementConfig.done = onSettlementDone;

    /*
     * Settlement must not create another pending reversal.
     * Preserve the original transaction identifiers.
     *
     * If your protocol requires settlement-specific composition,
     * use a dedicated composer instead of NULL. It must preserve
     * the original-data snapshot.
     */
    settlementConfig.beforeSend = NULL;
    settlementConfig.compose    = NULL;

    if (!txnFlowPrepare(&settlementFlow, NULL, &settlementConfig)) {
        LOG_TRACE("TxnMgr: settlement preparation failed");
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_PREPARE);
        return;
    }

    settlementData = mgr->data;

    settlementData.core.mti     = settlementConfig.mti;
    settlementData.core.txnType = settlementConfig.type;

    /* Attach after prepare(), which initializes the flow. */
    settlementFlow.data = &settlementData;

    char host[32] = {0};

    normalizeIp(settings()->server.mainServerIp, host, sizeof(host));

    settlementOperationId = operationId;
    settlementActive      = true;

    /*
     * connect() may synchronously call onSettlementDone().
     * Capture the operation ID so a callback starting another
     * transaction cannot cause the fallback to fail that transaction.
     */
    uint64_t startedOperation = operationId;

    LOG_TRACE("TxnMgr: starting settlement; operation=%llu",
              (unsigned long long)startedOperation);

    bool started = txnFlowConnect(&settlementFlow, host,
                                  settings()->server.mainServerPort);

    if (!started && operationId == startedOperation && settlementActive &&
        isSettlementStep(mgr)) {
        /*
         * connect() returned false without delivering completion.
         */
        settlementActive = false;

        LOG_TRACE("TxnMgr: settlement could not start");
        mgr->fail(TXN_MGR_ERR_SETTLEMENT_START);
    }
}

/******************************************************************
 *                      Transaction service steps
 ******************************************************************/

static TxnFlowConfig communicationCfg;
static char          communicationIp[32];

void communication(void) {
    LOG_TRACE("TxnMgr: communication step entered");
    TxnMgr* mgr = &__txnMgr;

    currentPendingSaved = false;

    /*
     * The manager owns completion for this flow.
     * Application result display belongs in onSuccess/onFailure.
     */
    communicationCfg            = *mgr->config.txnFlowCfg;
    communicationCfg.beforeSend = beforeCommunicationSend;
    communicationCfg.done       = mainTxnFlowDone;

    if (!txnFlowPrepare(mgr->flow, NULL, &communicationCfg)) {
        LOG_TRACE("TxnMgr: communication flow preparation failed");
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
        return;
    }
    mgr->flow->data        = &mgr->data;
    mgr->data.core.mti     = communicationCfg.mti;
    mgr->data.core.txnType = communicationCfg.type;

    mgr->flow->userData = mgr->userData;

    normalizeIp(settings()->server.mainServerIp, communicationIp,
                sizeof(communicationIp));

    LOG_TRACE("TxnMgr: starting transaction connection");
    communicationNotified = false;
    uint64_t startedId    = operationId;
    bool     started      = txnFlowConnect(mgr->flow, communicationIp,
                                           settings()->server.mainServerPort);

    LOG_TRACE(
        "TxnMgr: communication connect returned started=%d; operation=%llu",
        (int)started, (unsigned long long)startedId);

    if (!started && operationId == startedId && !communicationNotified &&
        mgr->active && isCommunicationStep(mgr)) {
        LOG_TRACE(
            "TxnMgr: communication start failed without completion callback");
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
    }
}

/**
 * @brief Request PIN only if required by the host.
 */
static void enterHostPin(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->hostPinRequired) {
        LOG_TRACE("TxnMgr: host PIN not required, skipping");
        mgr->next();
        return;
    }

    LOG_TRACE("TxnMgr: host PIN required");

    /*
     * Reuse the existing secure PIN input implementation.
     *
     * Its completion callback must recognize TXN_STEP_HOST_PIN
     * and advance this step after successful PIN entry.
     */
    enterPin();
}

/**
 * @brief Execute PIN authorization only when requested.
 */
static void communicationWithPin(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->hostPinRequired) {
        LOG_TRACE("TxnMgr: PIN communication not required, skipping");
        mgr->next();
        return;
    }
    mgr->authorizationAttempt++;
    communication();

    // LOG_TRACE("TxnMgr: PIN authorization integration pending");
    // mgr->fail(TXN_MGR_ERR_UNSUPPORTED_STEP);
}

/******************************************************************
 *                         Step dispatch
 ******************************************************************/

/* Private table does not require TxnStepMap in the public header.
 * Keep unfinished templates as documentation, but do not dispatch them. */
typedef struct {
    TxnMgrStep step;
    void (*handler)(void);
    bool implemented;
} ManagerStepEntry;

static const ManagerStepEntry stepMap[] = {
    {TXN_STEP_AMOUNT, enterAmount, true},
    {TXN_STEP_BILL_ID, enterBillId, true},
    {TXN_STEP_PAYMENT_ID, enterPaymentId, true},
    {TXN_STEP_OPERATOR, selectOperator, true},
    {TXN_STEP_CHARGE_AMOUNT, enterChargeAmount, true},
    {TXN_STEP_PHONE, enterPhone, true},
    {TXN_STEP_PIN, enterPin, true},
    {TXN_STEP_PENDING, checkPending, true},
    {TXN_STEP_COMMUNICATION, communication, true},
    {TXN_STEP_SETTLEMENT, settleCurrentTransaction, true},
    {TXN_STEP_NFC, getNfcData, true},
    {TXN_STEP_MAG, captureMagData, true},
    {TXN_STEP_HOST_PIN, enterHostPin, true},
    {TXN_STEP_COMMUNICATION_PIN, communicationWithPin, true},
};

/* Find and execute the handler for the requested logical step. */
static void onStep(TxnMgrStep step) {
    for (size_t i = 0; i < ARRAY_COUNT(stepMap); ++i) {
        if (stepMap[i].step == step) {
            if (stepMap[i].handler && stepMap[i].implemented) {
                stepMap[i].handler();
                return;
            }

            break;
        }
    }

    /* Missing or unassigned handlers terminate the transaction. */
    LOG_TRACE("TxnMgr: step missing or unimplemented; step=%d", (int)step);
    __txnMgr.fail(TXN_MGR_ERR_UNSUPPORTED_STEP);
}

/*
 * Start the current logical step once.
 *
 * stepStarted is set before calling the handler because a handler
 * may complete synchronously and call next() or fail().
 */
static void executeStep(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active || mgr->stepStarted || !mgr->onStep ||
        mgr->stepIndex >= mgr->stepCount) {
        LOG_TRACE("TxnMgr: dispatch skipped; active=%d, started=%d, index=%lu, "
                  "count=%lu",
                  (int)mgr->active, (int)mgr->stepStarted,
                  (unsigned long)mgr->stepIndex, (unsigned long)mgr->stepCount);
        return;
    }

    mgr->step        = mgr->steps[mgr->stepIndex];
    mgr->stepStarted = true;

    LOG_TRACE("TxnMgr: dispatch operation=%llu, step=%d, index=%lu",
              (unsigned long long)operationId, (int)mgr->step,
              (unsigned long)mgr->stepIndex);
    mgr->onStep(mgr->step);
}

/******************************************************************
 *                       State callbacks
 ******************************************************************/

STATE_DEF_EXIT(TxnMgr) {
    LOG_TRACE("TxnMgr: State exited; operation lifetime unchanged");
    (void)state;
    /* Temporary UI ownership changes do not finish the operation. */
}

STATE_DEF_ENTER(TxnMgr) {
    LOG_TRACE("TxnMgr: State entered");
    (void)state;

    executeStep();
}

/******************************************************************
 *                         Operations
 ******************************************************************/

/*
 * Reject only the requested operation.
 *
 * The existing manager state and active transaction are untouched.
 * TXN_STEP_IDLE means the requested transaction never started.
 */
static bool rejectRun(InfoDone done, TxnMgrError err) {
    LOG_TRACE("TxnMgr: run rejected; error=%d, callback=%d", (int)err,
              done != NULL);
    if (done) {
        TxnResultHandle resHandle = {.result     = TXN_MGR_RESULT_REJECT,
                                     .step       = TXN_STEP_IDLE,
                                     .err        = err,
                                     .flowStatus = NULL,
                                     .digitalRec = false,
                                     .data       = NULL,
                                     .onDone     = done};
        txnResultHandle(resHandle);
    }

    return false;
}

/*
 * Reset the manager to its initial idle condition.
 *
 * Returns false if transaction or recovery work is still active.
 * This is not a cancellation function.
 *
 * Clears transaction data and callbacks, but preserves:
 * - State and flow objects
 * - Operation function pointers
 * - Persisted pending transactions
 * - operationId, which must not be reused
 */
static bool reset(void) {
    TxnMgr* mgr = &__txnMgr;

    if (mgr->active || recoveryActive) {
        LOG_TRACE("TxnMgr: reset rejected; transaction or recovery active");
        return false;
    }

    if ((mgr->flow && (mgr->flow->tx || mgr->flow->stage != TXN_STAGE_IDLE)) ||
        recoveryFlow.tx || recoveryFlow.stage != TXN_STAGE_IDLE) {
        LOG_TRACE("TxnMgr: reset rejected; flow not released");
        return false;
    }

    memset(&mgr->data, 0, sizeof(mgr->data));

    mgr->config = (TxnMgrCfg){0};
    mgr->done   = NULL;

    mgr->flowStatus = (TxnFlowStatus){.result = TXN_FLOW_FAILED,
                                      .stage  = TXN_STAGE_IDLE,
                                      .code   = TXN_MGR_RESP_NONE};

    mgr->steps     = NULL;
    mgr->stepCount = 0;
    mgr->stepIndex = 0;

    mgr->step        = TXN_STEP_IDLE;
    mgr->stepStarted = false;
    mgr->active      = false;

    mgr->result        = TXN_MGR_RESULT_NONE;
    mgr->completedStep = TXN_STEP_IDLE;
    mgr->resultCode    = 0;

    currentPendingSaved   = false;
    communicationNotified = false;
    recoveryNotified      = false;

    mgr->authorizationAttempt = 0u;
    mgr->hostPinRequired      = false;

    mgr->userData = NULL;

    LOG_TRACE("TxnMgr: reset complete; no transaction started");
    return true;
}

/*
 * Start a transaction or report the exact rejection reason.
 *
 * Returns:
 *   false: Request rejected; done was called if provided.
 *   true:  Request accepted; completion is reported through done.
 *
 * Completion may happen synchronously while starting the first step.
 * A true return therefore does not mean the transaction succeeded
 * or is necessarily still running.
 *
 * When several conditions are invalid, the first check wins.
 */
static bool run(TxnMgrCfg* cfg, InfoDone done) {
    LOG_TRACE("TxnMgr: run requested. done cb address = %p", done);
    TxnMgr* mgr = &__txnMgr;

    if (!cfg) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_NULL_CONFIG");
        return rejectRun(done, TXN_MGR_ERR_NULL_CONFIG);
    }

    if (!cfg->txnFlowCfg) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_NULL_FLOW_CONFIG");
        return rejectRun(done, TXN_MGR_ERR_NULL_FLOW_CONFIG);
    }

    /*
     * Check recovery first because an operation waiting for recovery
     * normally also has mgr->active set.
     */
    if (recoveryActive) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_RECOVERY_BUSY");
        return rejectRun(done, TXN_MGR_ERR_RECOVERY_BUSY);
    }

    if (mgr->active) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_BUSY");
        return rejectRun(done, TXN_MGR_ERR_BUSY);
    }

    if (!mgr->onStep) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_NO_DISPATCHER");
        return rejectRun(done, TXN_MGR_ERR_NO_DISPATCHER);
    }

    if (!cfg->txnFlowCfg->build) {
        LOG_TRACE(
            "TxnMgr: run rejection reason: TXN_MGR_ERR_NO_BUILD_CALLBACK");
        return rejectRun(done, TXN_MGR_ERR_NO_BUILD_CALLBACK);
    }

    if (!cfg->txnFlowCfg->parse) {
        LOG_TRACE(
            "TxnMgr: run rejection reason: TXN_MGR_ERR_NO_PARSE_CALLBACK");
        return rejectRun(done, TXN_MGR_ERR_NO_PARSE_CALLBACK);
    }

    if (!mgr->flow) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_NULL_FLOW");
        return rejectRun(done, TXN_MGR_ERR_NULL_FLOW);
    }

    /*
     * Never prepare a flow that still owns transport resources
     * or has an unfinished communication stage.
     */
    if (mgr->flow->tx || mgr->flow->stage != TXN_STAGE_IDLE) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_FLOW_BUSY");
        return rejectRun(done, TXN_MGR_ERR_FLOW_BUSY);
    }

    if (recoveryFlow.tx || recoveryFlow.stage != TXN_STAGE_IDLE) {
        LOG_TRACE(
            "TxnMgr: run rejection reason: TXN_MGR_ERR_RECOVERY_FLOW_BUSY");
        return rejectRun(done, TXN_MGR_ERR_RECOVERY_FLOW_BUSY);
    }

    if (operationId == UINT64_MAX) {
        LOG_TRACE(
            "TxnMgr: run rejection reason: TXN_MGR_ERR_OPERATION_ID_EXHAUSTED");
        return rejectRun(done, TXN_MGR_ERR_OPERATION_ID_EXHAUSTED);
    }

    const TxnDefinition* def = findDefinition(cfg->txnFlowCfg->type);

    if (!def) {
        LOG_TRACE("TxnMgr: run rejection reason: "
                  "TXN_MGR_ERR_UNSUPPORTED_TRANSACTION");
        return rejectRun(done, TXN_MGR_ERR_UNSUPPORTED_TRANSACTION);
    }

    if (!def->steps || def->stepCount == 0) {
        LOG_TRACE("TxnMgr: run rejection reason: TXN_MGR_ERR_EMPTY_SEQUENCE");
        return rejectRun(done, TXN_MGR_ERR_EMPTY_SEQUENCE);
    }

    /*
     * TxnMgr installs its own persistence hook.
     * Do not silently replace an existing callback.
     */
    if (cfg->txnFlowCfg->beforeSend) {
        LOG_TRACE(
            "TxnMgr: run rejection reason: TXN_MGR_ERR_BEFORE_SEND_CONFLICT");
        return rejectRun(done, TXN_MGR_ERR_BEFORE_SEND_CONFLICT);
    }

    mgr->flowStatus = (TxnFlowStatus){.result = TXN_FLOW_FAILED,
                                      .stage  = TXN_STAGE_IDLE,
                                      .code   = TXN_MGR_RESP_NONE};

    if (cfg->txnFlowCfg->needSettlement) {
        bool hasPendingStep = false;

        for (size_t i = 0; i < def->stepCount; ++i) {
            if (def->steps[i] == TXN_STEP_PENDING) {
                hasPendingStep = true;
                break;
            }
        }

        if (!hasPendingStep) {
            LOG_TRACE("TxnMgr: run rejection reason: "
                      "TXN_MGR_ERR_PENDING_STEP_REQUIRED");
            return rejectRun(done, TXN_MGR_ERR_PENDING_STEP_REQUIRED);
        }
    }

    if (settlementActive || settlementFlow.tx ||
        settlementFlow.stage != TXN_STAGE_IDLE) {
        LOG_TRACE("TxnMgr: run rejected; settlement flow busy");
        return rejectRun(done, TXN_MGR_ERR_SETTLEMENT_BUSY);
    }

    /*
     * All rejection checks passed.
     * Only now modify the manager and install the new callback.
     */
    TxnMgrCfg requested = *cfg;

    operationCfg         = *cfg->txnFlowCfg;
    requested.txnFlowCfg = &operationCfg;

    ++operationId;

    currentPendingSaved   = false;
    communicationNotified = false;
    recoveryNotified      = false;

    mgr->authorizationAttempt = 0u;
    mgr->hostPinRequired      = false;

    memset(&mgr->data, 0, sizeof(mgr->data));

    mgr->data.status = TXN_STATUS_DECLINED;

    mgr->config            = requested;
    mgr->done              = done;
    mgr->data.core.txnType = operationCfg.type;

    mgr->steps       = def->steps;
    mgr->stepCount   = def->stepCount;
    mgr->stepIndex   = 0;
    mgr->step        = def->steps[0];
    mgr->stepStarted = false;
    mgr->active      = true;

    mgr->result        = TXN_MGR_RESULT_RUNNING;
    mgr->completedStep = TXN_STEP_IDLE;
    mgr->resultCode    = 0;

    /*
     * The manager's State entry dispatches the first step.
     * Same-State restart handling still depends on your SM API.
     */
    LOG_TRACE(
        "TxnMgr: run accepted; operation=%llu, type=%d, steps=%lu, done=%d",
        (unsigned long long)operationId, (int)operationCfg.type,
        (unsigned long)mgr->stepCount, done != NULL);
    SM_GOTO(mgr->smState);

    return true;
}

/*
 * Record the outcome before notifying the caller.
 *
 * err describes a local manager failure.
 * respCode is a valid host response, or TXN_MGR_RESP_NONE.
 *
 * A host decline is FAILURE with err == TXN_MGR_ERR_RESP_CODE.
 */
void complete(TxnMgrResult result, TxnMgrError err, TxnFlowStatus* status) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active) {
        LOG_TRACE("TxnMgr: completion ignored: no active operation");
        return;
    }

    if (status) {
        mgr->flowStatus = *status;
    } else {
        mgr->flowStatus = (TxnFlowStatus){.result = TXN_FLOW_FAILED,
                                          .stage  = TXN_STAGE_IDLE,
                                          .code   = TXN_MGR_RESP_NONE};
    }

    status = &mgr->flowStatus;

    TxnMgrStep completedStep = mgr->step;
    LOG_TRACE("TxnMgr: complete operation=%llu, result=%d, step=%d, error=%d, "
              "response=%d",
              (unsigned long long)operationId, (int)result, (int)completedStep,
              (int)err, status->code);

    /*
     * Capture callbacks before notification: a callback may start
     * another transaction and replace the manager configuration.
     */
    InfoDone        done      = mgr->done;
    TxnMgrSuccessCb onSuccess = mgr->config.onSuccess;
    TxnMgrFailureCb onFailure = mgr->config.onFailure;

    /* Retain the existing combined code for legacy callbacks. */
    int code = err != TXN_MGR_ERR_RESP_CODE ? (int)err : status->code;

    mgr->result        = result;
    mgr->completedStep = completedStep;
    mgr->resultCode    = code;

    mgr->active      = false;
    mgr->stepStarted = false;
    mgr->step        = TXN_STEP_IDLE;

    mgr->steps     = NULL;
    mgr->stepCount = 0;
    mgr->stepIndex = 0;
    mgr->done      = NULL;

    LOG_TRACE("amount = %llu", mgr->data.core.amount);

    /*
     * Keep transaction data available to the callback.
     * Do not modify manager state after invoking a callback.
     */
    if (done) {
        LOG_TRACE("TxnMgr: notifying done callback");
        TxnResultHandle resHandle = {.result     = result,
                                     .step       = completedStep,
                                     .err        = err,
                                     .flowStatus = status,
                                     .digitalRec =
                                         mgr->config.txnFlowCfg->digitalReceipt,
                                     .data   = &mgr->data,
                                     .onDone = done};
        txnResultHandle(resHandle);
        return;
    }

    if (result == TXN_MGR_RESULT_SUCCESS) {
        if (onSuccess) {
            LOG_TRACE("TxnMgr: notifying legacy success callback");
            onSuccess();
        }
    } else if (onFailure) {
        LOG_TRACE("TxnMgr: notifying legacy failure callback");
        onFailure(completedStep, code);
    }
}

/*
 * Report successful completion of the current step.
 *
 * Intermediate success immediately dispatches the next step.
 * Final-step success completes the transaction.
 *
 * Returns whether the step completion was accepted.
 */
static bool next(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active || !mgr->stepStarted) {
        LOG_TRACE("TxnMgr: next rejected; active=%d, started=%d",
                  (int)mgr->active, (int)mgr->stepStarted);
        return false;
    }

    if (settlementActive || settlementFlow.tx ||
        settlementFlow.stage != TXN_STAGE_IDLE) {
        LOG_TRACE("TxnMgr: run rejected; settlement flow busy");
        return false;
    }

    LOG_TRACE("TxnMgr: step succeeded; step=%d", (int)mgr->step);
    if (mgr->stepIndex + 1 >= mgr->stepCount) {
        complete(TXN_MGR_RESULT_SUCCESS, TXN_MGR_ERR_NONE, &mgr->flowStatus);

        return true;
    }

    ++mgr->stepIndex;

    mgr->step        = mgr->steps[mgr->stepIndex];
    mgr->stepStarted = false;

    executeStep();

    return true;
}

/*
 * Restart the current logical step.
 *
 * Preserves transaction data. The handler decides which input to reset.
 * Stop the current input provider and its timers before calling.
 *
 * Recovery and communication require a separate retry policy.
 */
static bool retry(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active || !mgr->stepStarted) {
        LOG_TRACE("TxnMgr: retry rejected; active=%d, started=%d",
                  (int)mgr->active, (int)mgr->stepStarted);
        return false;
    }

    if (!mgr->steps || mgr->stepIndex >= mgr->stepCount) {
        LOG_TRACE("TxnMgr: retry rejected; invalid step sequence");
        return false;
    }

    TxnMgrStep step = mgr->steps[mgr->stepIndex];

    if (step == TXN_STEP_PENDING || step == TXN_STEP_COMMUNICATION) {
        LOG_TRACE("TxnMgr: retry rejected; step=%d requires recovery policy",
                  (int)step);
        return false;
    }

    LOG_TRACE("TxnMgr: retrying step=%d", (int)step);

    mgr->step        = step;
    mgr->stepStarted = false;

    executeStep();

    return true;
}

/*
 * Return to the previous logical step.
 *
 * Preserves transaction data; the previous handler decides whether
 * to reuse or clear its input.
 *
 * Stop the current input provider and its timers before calling.
 * Do not use this to rewind pending recovery or communication.
 */
static bool previous(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active || !mgr->stepStarted) {
        LOG_TRACE("TxnMgr: previous rejected; active=%d, started=%d",
                  (int)mgr->active, (int)mgr->stepStarted);
        return false;
    }

    if (!mgr->steps || mgr->stepIndex >= mgr->stepCount) {
        LOG_TRACE("TxnMgr: previous rejected; invalid step sequence");
        return false;
    }

    if (mgr->stepIndex == 0) {
        LOG_TRACE("TxnMgr: already at first step. retrying first step");
        retry();
        return false;
    }

    /*
     * These steps may have network or persistence side effects.
     * Returning to input requires an explicit cancellation/recovery
     * policy rather than simply decrementing the index.
     */
    if (mgr->step == TXN_STEP_PENDING || mgr->step == TXN_STEP_COMMUNICATION) {
        LOG_TRACE("TxnMgr: previous rejected; step=%d cannot be rewound",
                  (int)mgr->step);
        return false;
    }

    TxnMgrStep previousStep = mgr->steps[mgr->stepIndex - 1];

    if (previousStep == TXN_STEP_PENDING ||
        previousStep == TXN_STEP_COMMUNICATION) {
        LOG_TRACE("TxnMgr: previous rejected; target step=%d",
                  (int)previousStep);
        return false;
    }

    LOG_TRACE("TxnMgr: previous; from=%d, to=%d", (int)mgr->step,
              (int)previousStep);

    --mgr->stepIndex;

    mgr->step        = previousStep;
    mgr->stepStarted = false;

    executeStep();

    return true;
}

/* Local failure without a valid host response. */
static void fail(TxnMgrError err) {
    LOG_TRACE("TxnMgr: failure reported; error=%d", (int)err);
    complete(TXN_MGR_RESULT_FAILURE, err, &__txnMgr.flowStatus);
}

/******************************************************************
 *                       Initialization
 ******************************************************************/

OOP_CTOR(TxnMgr) {
    LOG_TRACE("TxnMgr: initializing singleton");
    self->smState = &__txnMgrState;
    self->flow    = &__flow;

    self->step        = TXN_STEP_IDLE;
    self->active      = false;
    self->stepStarted = false;

    self->steps     = NULL;
    self->stepCount = 0;
    self->stepIndex = 0;

    self->run      = run;
    self->next     = next;
    self->previous = previous;
    self->retry    = retry;
    self->reset    = reset;
    self->fail     = fail;
    self->onStep   = onStep;
    self->done     = NULL;

    self->config = (TxnMgrCfg){0};

    self->result        = TXN_MGR_RESULT_NONE;
    self->completedStep = TXN_STEP_IDLE;
    self->resultCode    = 0;

    self->pendingNew    = pendingNew;
    self->pendingUpdate = pendingUpdate;
    self->pendingMark   = pendingMark;
    self->pendingReset  = pendingReset;
    self->pendingCheck  = pendingCheck;

    self->authorizationAttempt = 0u;
    self->hostPinRequired      = false;

    recoveryActive = false;

    txnFlowInit(&recoveryFlow);

    memset(&self->data, 0, sizeof(self->data));

    txnFlowInit(self->flow);
    currentPendingSaved   = false;
    communicationNotified = false;
    recoveryNotified      = false;
    operationId           = 0;

    txnFlowInit(&settlementFlow);
    settlementActive      = false;
    settlementOperationId = 0;

    OOP_CALL_CTOR(State, self->smState, NULL, "transaction manager");

    self->smState->vtable.enter = STATE_ENTER(TxnMgr);
    self->smState->vtable.exit  = STATE_EXIT(TxnMgr);
}

/* Return the single manager instance, constructing it once. */
TxnMgr* txnMgr(void) {
    CALL_ONCE(OOP_CALL_CTOR(TxnMgr, &__txnMgr););

    return &__txnMgr;
}