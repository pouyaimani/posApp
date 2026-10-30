#ifndef TXN_DEF_H_
#define TXN_DEF_H_

#include "txnMgr.h"

extern const TxnMgrCfg balanceTxnCfg;
extern const TxnMgrCfg billTxnCfg;
extern const TxnMgrCfg cfgTxnCfg;
extern const TxnMgrCfg logonTxnCfg;
extern const TxnMgrCfg purchaseTxnCfg;
extern const TxnMgrCfg reverseTxnCfg;
extern const TxnMgrCfg settlementTxnCfg;
extern const TxnMgrCfg topupTxnCfg;
extern const TxnMgrCfg voucherTxnCfg;
extern const TxnMgrCfg kahrobaPurchaseTxnCfg;
extern const TxnMgrCfg kahrobaBalanceTxnCfg;

const TxnDefinition* findDefinition(TxnType type);

#endif