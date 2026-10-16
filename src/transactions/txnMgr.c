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

#include "inputMgr/inputMgr.h"
#include "ui/infoPage.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "infoMgr/infoMgr.h"

/* True only while the manager is waiting for recovery completion. */
extern bool recoveryActive;

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

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

/******************************************************************
 *                      Transaction definitions
 ******************************************************************/

/*
 * Each array defines the logical execution order for one transaction.
 *
 * A step reports success through next() or terminal failure through
 * fail(code). Internal steps do not require separate State objects.
 */
static const TxnMgrStep purchaseSteps[] = {
    TXN_STEP_AMOUNT, TXN_STEP_PIN, TXN_STEP_PENDING, TXN_STEP_COMMUNICATION,
    TXN_STEP_PENDING};

static const TxnMgrStep balanceSteps[] = {TXN_STEP_PIN, TXN_STEP_COMMUNICATION};

static const TxnMgrStep billSteps[] = {TXN_STEP_BILL_ID, TXN_STEP_PAYMENT_ID,
                                       TXN_STEP_PIN, TXN_STEP_PENDING,
                                       TXN_STEP_COMMUNICATION};

static const TxnMgrStep voucherSteps[] = {
    TXN_STEP_OPERATOR, TXN_STEP_CHARGE_AMOUNT, TXN_STEP_PIN, TXN_STEP_PENDING,
    TXN_STEP_COMMUNICATION};

static const TxnMgrStep topupSteps[] = {
    TXN_STEP_OPERATOR, TXN_STEP_CHARGE_AMOUNT, TXN_STEP_PHONE,
    TXN_STEP_PIN,      TXN_STEP_PENDING,       TXN_STEP_COMMUNICATION};

static const TxnMgrStep configSteps[] = {TXN_STEP_COMMUNICATION};
static const TxnMgrStep logonSteps[]  = {TXN_STEP_COMMUNICATION};

typedef struct {
    TxnType           type;
    const TxnMgrStep* steps;
    size_t            stepCount;
} TxnDefinition;

#define TXN_DEFINITION(type_, steps_) {(type_), (steps_), ARRAY_COUNT(steps_)}

static const TxnDefinition definitions[] = {
    TXN_DEFINITION(TXN_PURCHASE, purchaseSteps),
    TXN_DEFINITION(TXN_BALANCE, balanceSteps),
    TXN_DEFINITION(TXN_BILL, billSteps),
    TXN_DEFINITION(TXN_VOUCHER, voucherSteps),
    TXN_DEFINITION(TXN_TOPUP, topupSteps),
    TXN_DEFINITION(TXN_CFG, configSteps),
    TXN_DEFINITION(TXN_LOGON, logonSteps)};

/*
 * Return the definition for a supported transaction.
 * The returned definition and its step array have static lifetime.
 */
static const TxnDefinition* findDefinition(TxnType type) {
    for (size_t i = 0; i < ARRAY_COUNT(definitions); ++i) {
        if (definitions[i].type == type) {
            return &definitions[i];
        }
    }

    return NULL;
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
        mgr->step != TXN_STEP_COMMUNICATION || !mgr->stepStarted) {
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

/*
 * Complete the current transaction after TxnFlow releases its
 * communication resources.
 */
static void txnFlowDone(TxnFlow* flow, const TxnFlowStatus* status) {
    LOG_TRACE("Txn Mgr: txnFlowDone ...");
    LOG_TRACE("Txn Mgr: respcode = %d", flow->data->core.respCode);

    TxnMgr* mgr = &__txnMgr;

    if (!flow || flow != mgr->flow || !mgr->active || !mgr->stepStarted ||
        mgr->step != TXN_STEP_COMMUNICATION) {
        LOG_TRACE(
            "TxnMgr: ignoring communication completion outside active step");
        return;
    }

    communicationNotified = true;
    if (!status) {
        LOG_TRACE("TxnMgr: communication completion has no status");
        mgr->fail(ERR_NOK);
        return;
    }
    LOG_TRACE("TxnMgr: communication result=%d, stage=%d, code=%d",
              (int)status->result, (int)status->stage, status->code);

    LOG_TRACE("mgr->data->core.pan = %s", mgr->data.core.pan);

    mgr->flowStatus = *status;

    if (status->result != TXN_FLOW_SUCCESS) {
        mgr->pendingReset();
        /* No valid host response: code may be a transport/parser error. */
        mgr->fail(status->result == TXN_FLOW_TIMEOUT
                      ? TXN_MGR_ERR_TIMEOUT
                      : TXN_MGR_ERR_COMMUNICATION);
        return;
    }

    if (status->code != RESP_CODE_SUCESS) {
        LOG_TRACE("TxnMgr: host declined; response=%d", status->code);
        /*
         * A valid host decline is not a local manager error.
         * Preserve the existing recovery-record policy here.
         */
        mgr->pendingReset();
        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_RESP_CODE, status);
        return;
    }

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
    }

    LOG_TRACE("TxnMgr: communication completed successfully");
    mgr->next();
}

/******************************************************************
 *                      Transaction service steps
 ******************************************************************/

static TxnFlowConfig communicationCfg;
static char          communicationIp[32];

static void communication(void) {
    LOG_TRACE("TxnMgr: communication step entered");
    TxnMgr* mgr = &__txnMgr;

    currentPendingSaved = false;

    /*
     * The manager owns completion for this flow.
     * Application result display belongs in onSuccess/onFailure.
     */
    communicationCfg            = *mgr->config.txnFlowCfg;
    communicationCfg.beforeSend = beforeCommunicationSend;
    communicationCfg.done       = txnFlowDone;

    if (!txnFlowPrepare(mgr->flow, NULL, &communicationCfg)) {
        LOG_TRACE("TxnMgr: communication flow preparation failed");
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
        return;
    }
    mgr->flow->data        = &mgr->data;
    mgr->data.core.mti     = communicationCfg.mti;
    mgr->data.core.txnType = communicationCfg.type;

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
        mgr->active && mgr->step == TXN_STEP_COMMUNICATION) {
        LOG_TRACE(
            "TxnMgr: communication start failed without completion callback");
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
    }
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
    {TXN_STEP_BILL_ID, enterBillId, false},
    {TXN_STEP_PAYMENT_ID, enterPaymentId, false},
    {TXN_STEP_OPERATOR, selectOperator, false},
    {TXN_STEP_CHARGE_AMOUNT, enterChargeAmount, false},
    {TXN_STEP_PHONE, enterPhone, false},
    {TXN_STEP_PIN, enterPin, true},
    {TXN_STEP_PENDING, checkPending, true},
    {TXN_STEP_COMMUNICATION, communication, true}};

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
                                     .flowStatus = &__txnMgr.flowStatus,
                                     .digitalRec = false,
                                     .data       = &__txnMgr.data,
                                     .onDone     = __txnMgr.done};
        txnResultHandle(resHandle);
    }

    return false;
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

    memset(&mgr->data, 0, sizeof(mgr->data));

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
    HIDE_INFO();
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active) {
        LOG_TRACE("TxnMgr: completion ignored: no active operation");
        return;
    }

    TxnMgrStep completedStep = mgr->step;
    LOG_TRACE("TxnMgr: complete operation=%llu, result=%d, step=%d, error=%d, "
              "response=%d",
              (unsigned long long)operationId, (int)result, (int)completedStep,
              (int)err, status->code);

    /*
     * Capture callbacks before notification: a callback may start
     * another transaction and replace the manager configuration.
     */
    LOG_TRACE("mgr->data->core.pan = %s", mgr->data.core.pan);
    LOG_TRACE("mgr Mgr: respcode = %d", mgr->data.core.respCode);
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

    self->run    = run;
    self->next   = next;
    self->fail   = fail;
    self->onStep = onStep;
    self->done   = NULL;

    self->config = (TxnMgrCfg){0};

    self->result        = TXN_MGR_RESULT_NONE;
    self->completedStep = TXN_STEP_IDLE;
    self->resultCode    = 0;

    self->pendingNew    = pendingNew;
    self->pendingUpdate = pendingUpdate;
    self->pendingMark   = pendingMark;
    self->pendingReset  = pendingReset;
    self->pendingCheck  = pendingCheck;

    recoveryActive = false;

    txnFlowInit(&recoveryFlow);

    memset(&self->data, 0, sizeof(self->data));

    txnFlowInit(self->flow);
    currentPendingSaved   = false;
    communicationNotified = false;
    recoveryNotified      = false;
    operationId           = 0;

    OOP_CALL_CTOR(State, self->smState, NULL, "transaction manager");

    self->smState->vtable.enter = STATE_ENTER(TxnMgr);
    self->smState->vtable.exit  = STATE_EXIT(TxnMgr);
}

/* Return the single manager instance, constructing it once. */
TxnMgr* txnMgr(void) {
    CALL_ONCE(OOP_CALL_CTOR(TxnMgr, &__txnMgr););

    return &__txnMgr;
}