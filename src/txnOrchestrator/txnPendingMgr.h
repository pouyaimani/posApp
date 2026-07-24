#ifndef TXNRECOVERY_H_
#define TXNRECOVERY_H_

#include "txn.h"
#include "txnFlow.h"

typedef int (*TxnPendMgrCb)();

typedef struct {

    void (*run)(TxnPendMgrCb);
    void (*start)(TxnData*);
    void (*approve)(TxnData*, bool needSettle);
    void (*decline)(TxnData*);
    void (*markReverse)(TxnData*);
    void (*markSettlement)(TxnData*);

    bool (*hasPendingTxn)(void);

    bool (*getPendingReverse)(TxnData*);

    bool (*getPendingSettle)(TxnData*);

} TxnPendingMgr;

TxnPendingMgr* txnPendingMgr(void);

#endif