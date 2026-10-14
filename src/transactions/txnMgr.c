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

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "inputMgr/inputMgr.h"
#include "ui/infoPage.h"
#include "storage/storage.h"
#include "settings/settings.h"

#define RECOVERY_TXN_DATA_ADDR "/mtd0/txn_rec"
#define TAG_PENDING_TXN        0x0501

extern const TxnFlowConfig settlementTxnFlowCfg;
extern const TxnFlowConfig reverseTxnFlowCfg;

/*
 * Recovery uses separate storage from the new transaction.
 *
 * __txnMgr.data and __txnMgr.flow belong to the new operation.
 * pendingData and recoveryFlow belong to the older operation.
 */
static TxnData pendingData;
static TxnFlow recoveryFlow;

/* True only while the manager is waiting for recovery completion. */
static bool recoveryActive;

static const DataDescriptor pendingTxnDsc[] = {
    DSC_BIN_ID(TAG_PENDING_TXN, pendingData)};

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static TxnMgr  __txnMgr;
static State   __txnMgrState;
static TxnFlow __flow;

/* Snapshot configuration; caller may release its configuration structure. */
static TxnFlowConfig operationCfg;
static TxnFlowConfig recoveryCfg;

/* Guard fallback handling after a synchronous completion starts another run. */
static uint64_t operationId;
static bool     communicationNotified;
static bool     recoveryNotified;

/* All manager entry points and provider callbacks must be serialized.
 * Provider cancellation and stale-event filtering remain provider contracts.
 * See TODO comments before enabling the unimplemented transaction steps.
 */

/*
 * Persistent input configuration and output storage.
 *
 * The configuration remains valid while InputMgr is running, even if
 * InputMgr retains its pointer rather than copying the structure.
 *
 * Reserve an additional byte for the amount string terminator.
 */
static InputCfg inputCfg;
static char     amountStr[LEN_MAX_AMOUNT + 1];

/******************************************************************
 *                      Forward declarations
 ******************************************************************/

static void executeStep(void);
void        settleDone(TxnFlow* flow, const TxnFlowStatus* status);
void        reverseDone(TxnFlow* flow, const TxnFlowStatus* status);

static void enterAmount(void);
static void enterBillId(void);
static void enterPaymentId(void);
static void selectOperator(void);
static void enterChargeAmount(void);
static void enterPhone(void);
static void enterPin(void);
static void checkPending(void);
static void communication(void);

static void onInputComplete(InputResult result, void* userData);
static void onInputTimeout(InputResult result, void* userData);

static void complete(TxnMgrResult result, TxnMgrError err, int respCode);

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
    TXN_STEP_AMOUNT, TXN_STEP_PIN, TXN_STEP_PENDING, TXN_STEP_COMMUNICATION};

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
    TXN_DEFINITION(TXN_CFG, configSteps)};

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
 *                      Input result handling
 ******************************************************************/

/*
 * Recognize only input steps currently connected to these callbacks.
 *
 * Extend this function when the remaining input handlers are
 * implemented and connected to onInputComplete().
 *
 * This is a step guard, not protection against callbacks belonging
 * to an older transaction. InputMgr must stop delivering old events
 * before a new transaction starts.
 */
static bool isInputStep(const TxnMgr* mgr) {
    if (!mgr || !mgr->active || !mgr->stepStarted) {
        return false;
    }

    return mgr->step == TXN_STEP_AMOUNT || mgr->step == TXN_STEP_PIN;
}

/*
 * Validate and store the amount before advancing to PIN.
 *
 * The minimum-length rule preserves the existing behavior.
 * Add transaction-specific numeric limits here when available.
 */
static bool saveAmount(TxnMgr* mgr) {
    uint64_t amount = 0;
    size_t   len    = 0;

    if (!mgr) {
        return false;
    }

    /* Bounded scan: reject missing terminators, signs, separators and overflow.
     */
    while (len < sizeof(amountStr) && amountStr[len] != '\0') {
        unsigned char ch = (unsigned char)amountStr[len];
        if (ch < '0' || ch > '9') {
            return false;
        }
        unsigned digit = (unsigned)(ch - '0');
        if (amount > (UINT64_MAX - digit) / 10) {
            return false;
        }
        amount = amount * 10 + digit;
        ++len;
    }

    if (len == sizeof(amountStr) || len > LEN_MAX_AMOUNT ||
        len < LEN_MIN_AMOUNT || amount == 0) {
        return false;
    }

    /* TODO: Apply numeric minimum/maximum rules for your payment protocol. */
    mgr->data.core.amount = amount;
    return true;
}

/*
 * Handle completion from the currently active input provider.
 *
 * InputMgr must commit its output buffer before invoking this
 * callback. Its provider cleanup must also permit starting another
 * input operation from next().
 */
static void onInputComplete(InputResult result, void* userData) {
    TxnMgr* mgr = userData;

    if (!isInputStep(mgr)) {
        return;
    }

    switch (result) {
    case INPUT_RES_FINISHED:
        switch (mgr->step) {
        case TXN_STEP_AMOUNT:
            if (!saveAmount(mgr)) {
                mgr->fail(TXN_MGR_ERR_INVALID_INPUT);
                return;
            }
            break;

        case TXN_STEP_PIN:
            /*
             * PIN processing belongs to the PED provider.
             * Do not copy plaintext PIN input into transaction data.
             *
             * TODO:
             * Confirm that INPUT_RES_FINISHED means the required
             * PED result is ready for transaction composition.
             */
            break;

        default:
            return;
        }

        mgr->next();
        return;

    case INPUT_RES_CANCELED:
        mgr->fail(TXN_MGR_ERR_CANCELED);
        return;

    default:
        /* Processing notifications do not complete the step. */
        return;
    }
}

/*
 * Report timeout through the same failure path as other errors.
 * The application's onFailure callback owns result navigation.
 */
static void onInputTimeout(InputResult result, void* userData) {
    TxnMgr* mgr = userData;

    (void)result;

    if (!isInputStep(mgr)) {
        return;
    }

    mgr->fail(TXN_MGR_ERR_TIMEOUT);
}

/******************************************************************
 *                         Input steps
 ******************************************************************/

static void enterAmount(void) {
    memset(amountStr, 0, sizeof(amountStr));

    inputCfg = (InputCfg){.mode    = INMD_ENTER_AMOUNT,
                          .title   = phraseGetDef(PHRASE_AMOUNT),
                          .info    = phraseGetDef(PHRASE_TO_RIAL_1),
                          .maxLen  = LEN_MAX_AMOUNT,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = &__txnMgr};

    inmgr()->run(&inputCfg, onInputComplete, &__txnMgr);

    /*
     * Preserve the supplied InputMgr API ordering.
     *
     * This requires run() not to deliver completion synchronously
     * before commit() installs the output buffer.
     */
    inmgr()->commit(amountStr, NULL, sizeof(amountStr));
}

static void enterBillId(void) {
    /*
     * TODO:
     * Start numeric input for the bill identifier.
     * Bind output to the appropriate TxnData bill field.
     *
     * On completion:
     *   - Validate the bill identifier.
     *   - Save the validated value.
     *   - Call next() on success or fail(code) on failure.
     *
     * Connect cancellation and timeout to fail().
     *
     * The current TxnData bill layout and validation declarations
     * are needed to implement this step.
     */
}

static void enterPaymentId(void) {
    /*
     * TODO:
     * Start numeric input for the payment identifier.
     * Bind output to the appropriate TxnData payment field.
     *
     * On completion:
     *   - Validate the payment identifier with the saved bill ID.
     *   - Extract and validate the bill amount.
     *   - Store identifiers and mgr->data.core.amount.
     *   - Call next() on success or fail(code) on failure.
     *
     * Connect cancellation and timeout to fail().
     */
}

static void selectOperator(void) {
    /*
     * TODO:
     * Open the operator selection menu.
     * Store the selected operator in the appropriate TxnData field.
     *
     * Call next() after a valid selection.
     * Call fail(code) on cancellation, timeout, or menu failure.
     *
     * The menu API and operator data definitions are needed here.
     */
}

static void enterChargeAmount(void) {
    /*
     * TODO:
     * Show the selected operator's supported charge amounts.
     * Support custom amount input if the operator allows it.
     *
     * Validate the selected amount and store it in transaction data,
     * including any operator-specific product or denomination field.
     *
     * Call next() on success or fail(code) on failure.
     */
}

static void enterPhone(void) {
    /*
     * TODO:
     * Start phone-number input.
     * Validate and normalize the number using project rules.
     * Store it in the appropriate TxnData field.
     *
     * Call next() on success.
     * Call fail(code) on invalid input, cancellation, or timeout.
     */
}

static void enterPin(void) {
    /*
     * Amount validation belongs to the amount step.
     * Balance and other transactions may reach PIN without amount
     * input, so do not inspect the previous input buffer here.
     */
    inputCfg = (InputCfg){.type    = INPUT_TYPE_PED,
                          .mode    = INMD_ENTER_PIN,
                          .title   = phraseGetDef(PHRASE_CARD_PIN),
                          .info    = phraseGetDef(PHRASE_BY_CUSTOMER),
                          .maxLen  = LEN_MAX_CARD_PIN,
                          .timeout = BACK_IDLE_TMO,
                          .tocb    = onInputTimeout,
                          .cbData  = &__txnMgr};

    /*
     * TODO:
     * Supply the balance fee message here if required.
     * Keep any dynamically formatted message valid for the complete
     * lifetime of the input screen.
     */

    inmgr()->run(&inputCfg, onInputComplete, &__txnMgr);
}

/******************************************************************
 *                  Pending transaction persistence
 ******************************************************************/

static bool pendingRequired(const TxnData* data) {
    return data && (data->status == TXN_STATUS_PENDING_REVERSE ||
                    data->status == TXN_STATUS_PENDING_SETTLEMENT);
}

/*
 * Load the persisted record.
 *
 * Storage errors must not be interpreted as "nothing pending".
 * No caller may inspect pendingData after a failed load.
 */
static Error_t pendingLoad(void) {
    TxnData previous = pendingData;
    Error_t err = storage()->load(pendingTxnDsc, ARRAY_COUNT(pendingTxnDsc),
                                  RECOVERY_TXN_DATA_ADDR);
    if (err != ERR_OK) {
        /* A failed load may have partially changed descriptor destinations. */
        pendingData = previous;
    }
    /* TODO: Map only a documented NOT_FOUND result to first-boot creation.
     * Do not treat malformed/corrupt data or IO failure as an empty record.
     * Validate stored enum/status values using the complete TxnStatus enum.
     */
    return err;
}

/*
 * Persist a complete record.
 *
 * Restore the in-memory record if saving fails.
 * Atomicity of the on-disk write remains the storage layer's
 * responsibility.
 */
static Error_t pendingSave(const TxnData* data) {
    if (!data) {
        return ERR_NOK;
    }

    TxnData previous = pendingData;

    pendingData = *data;

    Error_t err = storage()->save(pendingTxnDsc, ARRAY_COUNT(pendingTxnDsc),
                                  RECOVERY_TXN_DATA_ADDR);

    if (err != ERR_OK) {
        pendingData = previous;
    }

    return err;
}

/*
 * Store a transaction with a new recovery status.
 *
 * Update the caller's status only after persistence succeeds.
 * Do not overwrite the record while recovery is in progress.
 */
static Error_t pendingUpdate(TxnStatus status, TxnData* data) {
    if (!data || recoveryActive) {
        return ERR_NOK;
    }

    TxnData updated = *data;
    updated.status  = status;

    Error_t err = pendingSave(&updated);

    if (err == ERR_OK) {
        data->status = status;
    }

    return err;
}

/*
 * Record a new transaction requiring reversal if communication
 * does not reach the appropriate final outcome.
 *
 * Call only after the pending step has resolved the older record.
 */
static Error_t pendingNew(TxnData* data) {
    if (!data || recoveryActive) {
        return ERR_NOK;
    }

    /* Never replace an unresolved older record, even if a caller skipped
     * TXN_STEP_PENDING or supplied an inconsistent flow configuration. */
    Error_t err = pendingLoad();
    if (err != ERR_OK) {
        return err;
    }
    if (pendingRequired(&pendingData)) {
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
static Error_t pendingMark(TxnStatus status) {
    if (recoveryActive) {
        return ERR_NOK;
    }

    Error_t err = pendingLoad();

    if (err != ERR_OK) {
        return err;
    }

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
static Error_t pendingReset(void) {
    if (recoveryActive) {
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
static Error_t pendingCheck(bool* hasPending) {
    if (!hasPending) {
        return ERR_NOK;
    }

    *hasPending = false;

    if (recoveryActive) {
        return ERR_NOK;
    }

    Error_t err = pendingLoad();

    if (err != ERR_OK) {
        return err;
    }

    *hasPending = pendingRequired(&pendingData);

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
    TxnMgr* mgr = &__txnMgr;

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

    TxnMgr* mgr = &__txnMgr;

    if (!isRecoveryCompletion(flow)) {
        return;
    }

    /*
     * The matching recovery callback must correspond to the
     * operation selected from the stored record.
     */
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

        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_NONE, status->code);
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
        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_PENDING_SAVE,
                 status->code);
        return;
    }

    /*
     * Recovery is complete and durably recorded.
     * Continue the new transaction's sequence.
     */
    mgr->next();
}

void settleDone(TxnFlow* flow, const TxnFlowStatus* status) {
    recoveryDone(flow, status, TXN_STATUS_PENDING_SETTLEMENT,
                 TXN_STATUS_APPROVED);
}

void reverseDone(TxnFlow* flow, const TxnFlowStatus* status) {
    recoveryDone(flow, status, TXN_STATUS_PENDING_REVERSE, TXN_STATUS_REVERSED);
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
        return ERR_NOK;
    }

    if (!flow->cfg->needSettlement) {
        return ERR_OK;
    }

    Error_t err = mgr->pendingNew(&flow->data);

    if (err != ERR_OK) {
        return TXN_MGR_ERR_PENDING_SAVE;
    }

    currentPendingSaved = true;

    return ERR_OK;
}

/*
 * Complete the current transaction after TxnFlow releases its
 * communication resources.
 */
static void communicationDone(TxnFlow* flow, const TxnFlowStatus* status) {

    TxnMgr* mgr = &__txnMgr;

    if (!flow || flow != mgr->flow || !mgr->active || !mgr->stepStarted ||
        mgr->step != TXN_STEP_COMMUNICATION) {
        return;
    }

    communicationNotified = true;
    if (!status) {
        mgr->fail(ERR_NOK);
        return;
    }
    mgr->data = flow->data;

    if (status->result != TXN_FLOW_SUCCESS) {
        /* No valid host response: code may be a transport/parser error. */
        mgr->fail(status->result == TXN_FLOW_TIMEOUT
                      ? TXN_MGR_ERR_TIMEOUT
                      : TXN_MGR_ERR_COMMUNICATION);
        return;
    }

    if (status->code != RESP_CODE_SUCESS) {
        /*
         * A valid host decline is not a local manager error.
         * Preserve the existing recovery-record policy here.
         */
        complete(TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_NONE, status->code);
        return;
    }

    if (currentPendingSaved) {
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
            complete(
                TXN_MGR_RESULT_FAILURE, TXN_MGR_ERR_PENDING_SAVE,
                status->code); /* Host approved; local persistence failed. */
            return;
            return;
        }
    }

    mgr->next();
}

static void checkPending(void) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active || !mgr->stepStarted || mgr->step != TXN_STEP_PENDING ||
        recoveryActive) {
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
        cfg = &reverseTxnFlowCfg;
    } else if (pendingData.status == TXN_STATUS_PENDING_SETTLEMENT) {
        cfg = &settlementTxnFlowCfg;
    } else {
        /* No recovery is required; continue immediately. */
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
        mgr->fail(TXN_MGR_ERR_PENDING_PREPARE);
        return;
    }

    /*
     * Restore original transaction fields after preparation.
     * Override only the outgoing recovery operation identifiers.
     */
    recoveryFlow.data = pendingData;

    recoveryFlow.data.core.mti     = cfg->mti;
    recoveryFlow.data.core.txnType = cfg->type;

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
    recoveryActive = true;

    recoveryNotified   = false;
    uint64_t startedId = operationId;
    bool     started   = txnFlowConnect(&recoveryFlow, recoveryIp,
                                        settings()->server.mainServerPort);

    /* Some false returns invoke done synchronously, others do not.
     * Never fail a new operation started from a completion callback. */
    if (!started && operationId == startedId && !recoveryNotified &&
        mgr->active && recoveryActive) {
        recoveryActive = false;
        mgr->fail(TXN_MGR_ERR_PENDING_FLOW);
    }
}

/******************************************************************
 *                      Transaction service steps
 ******************************************************************/

static TxnFlowConfig communicationCfg;
static char          communicationIp[32];

static void communication(void) {
    TxnMgr* mgr = &__txnMgr;

    currentPendingSaved = false;

    /*
     * The manager owns completion for this flow.
     * Application result display belongs in onSuccess/onFailure.
     */
    communicationCfg            = *mgr->config.txnFlowCfg;
    communicationCfg.beforeSend = beforeCommunicationSend;
    communicationCfg.done       = communicationDone;

    if (!txnFlowPrepare(mgr->flow, NULL, &communicationCfg)) {
        mgr->fail(TXN_MGR_ERR_COMMUNICATION);
        return;
    }

    /* Restore the data collected by the manager's input steps. */
    mgr->flow->data = mgr->data;

    mgr->flow->data.core.mti     = communicationCfg.mti;
    mgr->flow->data.core.txnType = communicationCfg.type;

    normalizeIp(settings()->server.mainServerIp, communicationIp,
                sizeof(communicationIp));

    communicationNotified = false;
    uint64_t startedId    = operationId;
    bool     started      = txnFlowConnect(mgr->flow, communicationIp,
                                           settings()->server.mainServerPort);

    if (!started && operationId == startedId && !communicationNotified &&
        mgr->active && mgr->step == TXN_STEP_COMMUNICATION) {
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
        return;
    }

    mgr->step        = mgr->steps[mgr->stepIndex];
    mgr->stepStarted = true;

    mgr->onStep(mgr->step);
}

/******************************************************************
 *                       State callbacks
 ******************************************************************/

STATE_DEF_EXIT(TxnMgr) {
    (void)state;
    /* Temporary UI ownership changes do not finish the operation. */
}

STATE_DEF_ENTER(TxnMgr) {
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
static bool rejectRun(TxnMgrDoneCb done, TxnMgrError err) {
    if (done) {
        done(TXN_MGR_RESULT_REJECT, TXN_STEP_IDLE, err, TXN_MGR_RESP_NONE);
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
static bool run(TxnMgrCfg* cfg, TxnMgrDoneCb done) {
    TxnMgr* mgr = &__txnMgr;

    if (!cfg) {
        return rejectRun(done, TXN_MGR_ERR_NULL_CONFIG);
    }

    if (!cfg->txnFlowCfg) {
        return rejectRun(done, TXN_MGR_ERR_NULL_FLOW_CONFIG);
    }

    /*
     * Check recovery first because an operation waiting for recovery
     * normally also has mgr->active set.
     */
    if (recoveryActive) {
        return rejectRun(done, TXN_MGR_ERR_RECOVERY_BUSY);
    }

    if (mgr->active) {
        return rejectRun(done, TXN_MGR_ERR_BUSY);
    }

    if (!mgr->onStep) {
        return rejectRun(done, TXN_MGR_ERR_NO_DISPATCHER);
    }

    if (!cfg->txnFlowCfg->build) {
        return rejectRun(done, TXN_MGR_ERR_NO_BUILD_CALLBACK);
    }

    if (!cfg->txnFlowCfg->parse) {
        return rejectRun(done, TXN_MGR_ERR_NO_PARSE_CALLBACK);
    }

    if (!mgr->flow) {
        return rejectRun(done, TXN_MGR_ERR_NULL_FLOW);
    }

    /*
     * Never prepare a flow that still owns transport resources
     * or has an unfinished communication stage.
     */
    if (mgr->flow->tx || mgr->flow->stage != TXN_STAGE_IDLE) {
        return rejectRun(done, TXN_MGR_ERR_FLOW_BUSY);
    }

    if (recoveryFlow.tx || recoveryFlow.stage != TXN_STAGE_IDLE) {
        return rejectRun(done, TXN_MGR_ERR_RECOVERY_FLOW_BUSY);
    }

    if (operationId == UINT64_MAX) {
        return rejectRun(done, TXN_MGR_ERR_OPERATION_ID_EXHAUSTED);
    }

    const TxnDefinition* def = findDefinition(cfg->txnFlowCfg->type);

    if (!def) {
        return rejectRun(done, TXN_MGR_ERR_UNSUPPORTED_TRANSACTION);
    }

    if (!def->steps || def->stepCount == 0) {
        return rejectRun(done, TXN_MGR_ERR_EMPTY_SEQUENCE);
    }

    /*
     * TxnMgr installs its own persistence hook.
     * Do not silently replace an existing callback.
     */
    if (cfg->txnFlowCfg->beforeSend) {
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
    SM_GOTO(mgr->smState);

    return true;
}

/*
 * Record the outcome before notifying the caller.
 *
 * err describes a local manager failure.
 * respCode is a valid host response, or TXN_MGR_RESP_NONE.
 *
 * A host decline is FAILURE with err == TXN_MGR_ERR_NONE.
 */
static void complete(TxnMgrResult result, TxnMgrError err, int respCode) {
    TxnMgr* mgr = &__txnMgr;

    if (!mgr->active) {
        return;
    }

    TxnMgrStep completedStep = mgr->step;

    /*
     * Capture callbacks before notification: a callback may start
     * another transaction and replace the manager configuration.
     */
    TxnMgrDoneCb    done      = mgr->done;
    TxnMgrSuccessCb onSuccess = mgr->config.onSuccess;
    TxnMgrFailureCb onFailure = mgr->config.onFailure;

    /* Retain the existing combined code for legacy callbacks. */
    int code = err != TXN_MGR_ERR_NONE ? (int)err : respCode;

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
        done(result, completedStep, err, respCode);
        return;
    }

    if (result == TXN_MGR_RESULT_SUCCESS) {
        if (onSuccess) {
            onSuccess();
        }
    } else if (onFailure) {
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
        return false;
    }

    if (mgr->stepIndex + 1 >= mgr->stepCount) {
        complete(TXN_MGR_RESULT_SUCCESS, TXN_MGR_ERR_NONE, RESP_CODE_SUCESS);

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
    complete(TXN_MGR_RESULT_FAILURE, err, TXN_MGR_RESP_NONE);
}

/******************************************************************
 *                       Initialization
 ******************************************************************/

OOP_CTOR(TxnMgr) {
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