#ifndef TXN_FLOW_H
#define TXN_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "state.h"
#include "nth/nth.h"
#include "byteArray.h"
#include "txn.h"

typedef struct TxnFlow TxnFlow;

/* ================= Result ================= */

typedef enum {

    TXN_FLOW_SUCCESS = 0,

    TXN_FLOW_FAILED,

    TXN_FLOW_TIMEOUT

} TxnFlowResult;

/* ================= Stage ================= */

typedef enum {

    TXN_STAGE_IDLE = 0,

    TXN_STAGE_CONNECTING,

    TXN_STAGE_SENDING,

    TXN_STAGE_RECEIVING,

    TXN_STAGE_PARSING

} TxnFlowStage;

/* ================= Status ================= */

typedef struct {

    TxnFlowResult result;

    TxnFlowStage stage;

    int code;

} TxnFlowStatus;

/* ================= Callbacks ================= */

typedef int (*TxnBuildFn)(TxnFlow* flow, ByteArray* ba);

typedef int (*TxnParseFn)(TxnFlow* flow, ByteArray* ba);

typedef void (*TxnFlowDone)(TxnFlow* flow, const TxnFlowStatus* status);

typedef void (*TxnFlowStageCb)(TxnFlow* flow);

typedef int (*TxnComposeFn)(TxnData* data);

/* ================= Config ================= */

typedef struct {

    Mti_t mti;

    PrCode_t prcode;

    uint8_t* feilds;

    uint16_t feildsCnt;

    TxnBuildFn build;

    TxnParseFn parse;

    TxnFlowDone done;

    TxnFlowStageCb onConnecting;

    TxnFlowStageCb onSending;

    TxnFlowStageCb onReceiving;

    TxnComposeFn compose;

    bool needSettlement;

} TxnFlowConfig;

/* ================= Runtime ================= */

struct TxnFlow {

    State* owner;

    NthTransaction* tx;

    const TxnFlowConfig* cfg;

    TxnFlowStage stage;

    TxnData data;

    void* userData;
};

void txnFlowInit(TxnFlow* flow);

int8_t txnStart(TxnFlowConfig* cfg, TxnFlow* flow, State* owner);

bool txnRun(TxnFlow* flow, State* owner, const char* host, uint16_t port,
            const TxnFlowConfig* cfg);

void txnFlowRelease(TxnFlow* flow);

/*********************************************************************************************
 *                                                                                           *
 *                               Transactions config              *
 *                                                                                           *
 ********************************************************************************************/

extern const TxnFlowConfig logOnTxn;
extern const TxnFlowConfig cfgTxn;
extern const TxnFlowConfig balanceTxn;

/*********************************************************************************************
 *                                                                                           *
 *                               Transactions common              *
 *                                                                                           *
 ********************************************************************************************/
int  composeCommon(TxnData* data);
int  buildCommon(TxnFlow* flow, ByteArray* ba);
int  parseCommon(TxnFlow* flow, ByteArray* ba);
void showConnecting(TxnFlow* f);
void showSending(TxnFlow* f);
void showReceiving(TxnFlow* f);
void commonDone(TxnFlow* flow, const TxnFlowStatus* st, State* onSuc,
                State* onFail, bool showSucMsg);

#endif