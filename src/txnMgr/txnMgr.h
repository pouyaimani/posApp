#ifndef TXN_MGR_H_
#define TXN_MGR_H_

#include <stdbool.h>
#include <stddef.h>

#include "oop.h"
#include "state.h"
#include "txnFlow/txnFlow.h"
#include "infoMgr/infoMgr.h"
#include "emvMgr/emv_kahroba.h"

typedef enum {
    TXN_STEP_IDLE = 0,

    TXN_STEP_AMOUNT,
    TXN_STEP_BILL_ID,
    TXN_STEP_PAYMENT_ID,
    TXN_STEP_OPERATOR,
    TXN_STEP_PHONE,
    TXN_STEP_CHARGE_AMOUNT,
    TXN_STEP_PIN,
    TXN_STEP_PENDING,
    TXN_STEP_COMMUNICATION,
    TXN_STEP_NFC,
    TXN_STEP_MAG,
    TXN_STEP_SETTLEMENT,
    TXN_STEP_HOST_PIN,
    TXN_STEP_COMMUNICATION_PIN,
} TxnMgrStep;

typedef void (*TxnStepHandler)(void);

typedef struct {
    TxnMgrStep     step;
    TxnStepHandler handler;
} TxnStepMap;

typedef enum {
    TXN_MGR_RESULT_NONE = 0,
    TXN_MGR_RESULT_REJECT,
    TXN_MGR_RESULT_RUNNING,
    TXN_MGR_RESULT_SUCCESS,
    TXN_MGR_RESULT_FAILURE
} TxnMgrResult;

/* Manager-level errors; step handlers may also pass their own codes. */
typedef enum {
    TXN_MGR_ERR_NONE = 0,

    /* Transaction results */
    TXN_MGR_ERR_RESP_CODE,

    /* Input errors */
    TXN_MGR_ERR_INPUT,
    TXN_MGR_ERR_CANCELED,
    TXN_MGR_ERR_TIMEOUT,
    TXN_MGR_ERR_INVALID_INPUT,

    /* Existing step and persistence errors */
    TXN_MGR_ERR_UNSUPPORTED_STEP,
    TXN_MGR_ERR_PENDING_LOAD,
    TXN_MGR_ERR_PENDING_SAVE,
    TXN_MGR_ERR_PENDING_PREPARE,
    TXN_MGR_ERR_PENDING_FLOW,

    /* Communication errors */
    TXN_MGR_ERR_COMMUNICATION,

    /* Settlement errors */
    TXN_MGR_ERR_SETTLEMENT_CONTEXT,
    TXN_MGR_ERR_SETTLEMENT_BUSY,
    TXN_MGR_ERR_SETTLEMENT_PREPARE,
    TXN_MGR_ERR_SETTLEMENT_START,
    TXN_MGR_ERR_SETTLEMENT_FLOW,

    /* Configuration and initialization errors */
    TXN_MGR_ERR_NULL_CONFIG,
    TXN_MGR_ERR_NULL_FLOW_CONFIG,
    TXN_MGR_ERR_BUSY,
    TXN_MGR_ERR_RECOVERY_BUSY,
    TXN_MGR_ERR_NO_DISPATCHER,
    TXN_MGR_ERR_NO_BUILD_CALLBACK,
    TXN_MGR_ERR_NO_PARSE_CALLBACK,
    TXN_MGR_ERR_NULL_FLOW,
    TXN_MGR_ERR_FLOW_BUSY,
    TXN_MGR_ERR_RECOVERY_FLOW_BUSY,
    TXN_MGR_ERR_OPERATION_ID_EXHAUSTED,
    TXN_MGR_ERR_UNSUPPORTED_TRANSACTION,
    TXN_MGR_ERR_EMPTY_SEQUENCE,
    TXN_MGR_ERR_BEFORE_SEND_CONFLICT,
    TXN_MGR_ERR_PENDING_STEP_REQUIRED

} TxnMgrError;

typedef struct {
    TxnMgrResult   result;
    TxnMgrStep     step;
    TxnMgrError    err;
    TxnFlowStatus* flowStatus;
    bool           digitalRec;
    TxnData*       data;
    InfoDone       onDone;
} TxnResultHandle;

/* No valid host response was received. */
#define TXN_MGR_RESP_NONE (-1)

typedef void (*TxnMgrSuccessCb)(void);
typedef void (*TxnMgrFailureCb)(TxnMgrStep step, int code);

typedef void (*TxnMgrDoneCb)(TxnResultHandle handle);

typedef enum {
    TXN_HOST_APPROVE = 0,
    TXN_HOST_DECLINE,
    TXN_HOST_REQUIRE_PIN
} TxnHostAction;

/**
 * Interpret a host response for a specific transaction.
 *
 * attempt:
 *   0 = first authorization
 *   1 = second authorization
 */
typedef TxnHostAction (*TxnResponsePolicy)(TxnMgrStep step, int responseCode,
                                           uint8_t attempt);

typedef struct {
    TxnFlowConfig*    txnFlowCfg;
    TxnMgrSuccessCb   onSuccess;
    TxnMgrFailureCb   onFailure;
    TxnResponsePolicy responsePolicy;
} TxnMgrCfg;

typedef struct {
    TxnType           type;
    const TxnMgrStep* steps;
    size_t            stepCount;
} TxnDefinition;

OOP_CLASS(TxnMgr) {
    TxnMgrCfg config;

    InfoDone done;

    State*  smState;
    TxnData data;

    TxnFlow* flow;

    TxnFlowStatus flowStatus;
    TxnMgrStep    step;

    TxnMgrResult result;
    TxnMgrStep   completedStep;
    int          resultCode;

    const TxnMgrStep* steps;
    size_t            stepCount;
    size_t            stepIndex;

    bool active;
    bool stepStarted;

    void* userData;

    /* Current authorization attempt: 0 = initial request. */
    uint8_t authorizationAttempt;

    /* True when host requests online PIN authentication. */
    bool hostPinRequired;

    bool (*run)(TxnMgrCfg* cfg, InfoDone done);

    /*
     * Called after the current step succeeds.
     * Input must be validated and saved before calling next().
     */
    bool (*next)(void);

    bool (*previous)(void);

    bool (*retry)(void);

    bool (*reset)(void);

    /** Report a manager error and terminate the operation. */
    void (*fail)(TxnMgrError err);

    /*
     * Shared dispatcher for input, menus, and communication.
     * Assign once before calling run().
     */
    void (*onStep)(TxnMgrStep step);

    /*
     * Pending transaction persistence.
     *
     * These operations return storage errors to the caller.
     * The caller must not continue transmission after a required
     * persistence operation fails.
     */
    Error_t (*pendingNew)(TxnData* data);
    Error_t (*pendingUpdate)(TxnStatus status, TxnData* data);
    Error_t (*pendingMark)(TxnStatus status);
    Error_t (*pendingReset)(void);

    /*
     * Read pending state from storage.
     *
     * ERR_OK with *hasPending == false means no recovery is required.
     * A storage error is distinct from "no pending transaction".
     */
    Error_t (*pendingCheck)(bool* hasPending);
};

TxnMgr* txnMgr(void);

void txnResultHandle(TxnResultHandle handle);

#endif