#ifndef TXNRECOVERY_H_
#define TXNRECOVERY_H_

#include "txn.h"
#include "txnFlow.h"

typedef int (*TxnPendMgrCb)(const TxnData* txnData);

typedef struct {

    bool (*run)(TxnPendMgrCb);
    void (*mark)(TxnStatus st);
    void (*reset)();

    bool (*hasPendingTxn)(void);

} TxnPendingMgr;

TxnPendingMgr* txnPendingMgr(void);

#endif