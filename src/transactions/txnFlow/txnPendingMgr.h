#ifndef TXNRECOVERY_H_
#define TXNRECOVERY_H_

#include "txn.h"
#include "txnFlow.h"

typedef int (*TxnPendMgrCb)(bool result);

typedef struct {

    bool (*run)(TxnPendMgrCb);
    void (*mark)(TxnData*, TxnStatus st);

    bool (*hasPendingTxn)(void);

} TxnPendingMgr;

TxnPendingMgr* txnPendingMgr(void);

#endif