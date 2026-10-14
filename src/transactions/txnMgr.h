#ifndef TXN_MGR_H_
#define TXN_MGR_H_

#include <stdbool.h>
#include <stddef.h>

#include "oop.h"
#include "state.h"
#include "txnFlow/txnFlow.h"

typedef enum {
    TXN_STEP_IDLE = 0,
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
    TXN_MGR_ERR_PENDING_STEP_REQUIRED,
    TXN_STEP_AMOUNT,
    TXN_STEP_BILL_ID,
    TXN_STEP_PAYMENT_ID,
    TXN_STEP_OPERATOR,
    TXN_STEP_CHARGE_AMOUNT,
    TXN_STEP_PHONE,
    TXN_STEP_PIN,
    TXN_STEP_PENDING,
    TXN_STEP_COMMUNICATION
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
    TXN_MGR_ERR_INPUT,
    TXN_MGR_ERR_CANCELED,
    TXN_MGR_ERR_TIMEOUT,
    TXN_MGR_ERR_UNSUPPORTED_STEP,
    TXN_MGR_ERR_INVALID_INPUT,
    TXN_MGR_ERR_PENDING_LOAD,
    TXN_MGR_ERR_PENDING_SAVE,
    TXN_MGR_ERR_PENDING_PREPARE,
    TXN_MGR_ERR_PENDING_FLOW,
    TXN_MGR_ERR_COMMUNICATION
} TxnMgrError;

/* No valid host response was received. */
#define TXN_MGR_RESP_NONE (-1)

typedef void (*TxnMgrSuccessCb)(void);
typedef void (*TxnMgrFailureCb)(TxnMgrStep step, int code);

typedef void (*TxnMgrDoneCb)(TxnMgrResult result, TxnMgrStep step,
                             TxnMgrError err, int respCode);

typedef struct {
    TxnFlowConfig*  txnFlowCfg;
    TxnMgrSuccessCb onSuccess;
    TxnMgrFailureCb onFailure;
} TxnMgrCfg;

OOP_CLASS(TxnMgr) {
    TxnMgrCfg config;

    TxnMgrDoneCb done;

    State*  smState;
    TxnData data;

    TxnFlow*   flow;
    TxnMgrStep step;

    TxnMgrResult result;
    TxnMgrStep   completedStep;
    int          resultCode;

    const TxnMgrStep* steps;
    size_t            stepCount;
    size_t            stepIndex;

    bool active;
    bool stepStarted;

    bool (*run)(TxnMgrCfg* cfg, TxnMgrDoneCb done);

    /*
     * Called after the current step succeeds.
     * Input must be validated and saved before calling next().
     */
    bool (*next)(void);

    /* Report failure of any step and stop sequencing. */
    void (*fail)(int code);

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

#endif