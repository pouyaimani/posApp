#ifndef TXN_COMMON_H_
#define TXN_COMMON_H_

#include "txnFlow/txnFLow.h"

/*********************************************************************************************
 *                                                                                           *
 *                               Transactions config
 *                                                                                           *
 ********************************************************************************************/

extern const TxnFlowConfig logOnTxn;
extern const TxnFlowConfig cfgTxn;
extern const TxnFlowConfig balanceTxn;

/*********************************************************************************************
 *                                                                                           *
 *                               Transactions common
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
void financeTxnDone(TxnFlow* flow, const TxnFlowStatus* st);

#define TXN_FLOW_COMMON                                                        \
    .build = buildCommon, .parse = parseCommon,                                \
    .onConnecting = showConnecting, .onSending = showSending,                  \
    .onReceiving = showReceiving

#endif