#ifndef TRANSACTION_H_
#define TRANSACTION_H_

#include "state.h"
#include "dcfg.h"
#include "common.h"
#include "txnFlow/txnFLow.h"

/*********************Transaction*********************/

OOP_DECLARE_CLASS(Transaction)

OOP_VTABLE(Transaction) { OOP_IMETHOD(void, Transaction, makeReceipt); };

OOP_CLASS(Transaction) {
    OOP_IMPLEMENTS(Transaction);
    State    state;
    char     name[24];
    TxnFlow* flow;
};

OOP_CTOR(Transaction, State* parent, const char* name);

Transaction* getTxn(TxnType id);

/*********************Sale*********************/

OOP_CLASS(Purchase) { OOP_EXTENDS(Transaction); };

OOP_CTOR(Purchase, State* parent, const char* name);

/**********************Balance**********************/

OOP_CLASS(Balance) { OOP_EXTENDS(Transaction); };

OOP_CTOR(Balance, State* parent, const char* name);

/********************Bill**********************/

OOP_CLASS(Bill) { OOP_EXTENDS(Transaction); };

OOP_CTOR(Bill, State* parent, const char* name);

/********************Pay**********************/

OOP_CLASS(Pay) { OOP_EXTENDS(Transaction); };

OOP_CTOR(Pay, State* parent, const char* name);

/********************Voucher**********************/

OOP_CLASS(Voucher) { OOP_EXTENDS(Transaction); };

OOP_CTOR(Voucher, State* parent, const char* name);

/********************Topup**********************/

OOP_CLASS(TopUp) { OOP_EXTENDS(Transaction); };

OOP_CTOR(TopUp, State* parent, const char* name);

#endif