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
    bool     enable;
    char     name[24];
    TxnFlow* flow;
};

OOP_CTOR(Transaction, State* parent, const char* name);

typedef enum TxnId_t {
    TXN_ID_PURCHASE = 0,
    TXN_ID_BALANCE,
    TXN_ID_BILL,
    TXN_ID_PAY,
    TXN_ID_SIM_CHARGE,
    TXN_ID_ALL
} TxnId_t;

Transaction* getTxn(TxnId_t id);

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