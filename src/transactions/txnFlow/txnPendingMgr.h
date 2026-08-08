#ifndef TXNRECOVERY_H_
#define TXNRECOVERY_H_

#include "txn.h"
#include "txnFlow.h"
#include "error.h"

typedef int (*TxnPendMgrCb)(const TxnData* txnData);

typedef struct {

    bool (*run)(TxnPendMgrCb);
    Error_t (*mark)(TxnStatus st);
    Error_t (*new)(TxnData* txnData);
    Error_t (*update)(TxnStatus st, TxnData* txnData);
    Error_t (*reset)();

    bool (*hasPendingTxn)(void);

} TxnPendingMgr;

TxnPendingMgr* txnPendingMgr(void);

#endif