#ifndef SERVICES_H_
#define SERVICES_H_

#include "state.h"
#include "dcfg.h"
#include "common.h"
#include "txnOrchestrator/txnFLow.h"

extern int serviceCount;

/*********************Service*********************/

OOP_DECLARE_CLASS(Service)

OOP_VTABLE(Service) { OOP_IMETHOD(void, Service, makeReceipt); };

OOP_CLASS(Service) {
    OOP_IMPLEMENTS(Service);
    State   state;
    bool    enable;
    char    name[24];
    TxnFlow flow;
};

OOP_CTOR(Service, State* parent, const char* name);

typedef enum ServiceId_t {
    SERVICE_ID_SALE = 0,
    SERVICE_ID_BALANCE,
    SERVICE_ID_BILL,
    SERVICE_ID_PAY,
    SERVICE_ID_SIM_CHARGE,
    SERVICE_ID_ALL
} ServiceId_t;

Service* getService(ServiceId_t id);

/*********************Sale*********************/

OOP_CLASS(Sale) { OOP_EXTENDS(Service); };

OOP_CTOR(Sale, State* parent, const char* name);

/**********************Balance**********************/

OOP_CLASS(Balance) { OOP_EXTENDS(Service); };

OOP_CTOR(Balance, State* parent, const char* name);

/********************Bill**********************/

OOP_CLASS(Bill) { OOP_EXTENDS(Service); };

OOP_CTOR(Bill, State* parent, const char* name);

/********************Pay**********************/

OOP_CLASS(Pay) { OOP_EXTENDS(Service); };

OOP_CTOR(Pay, State* parent, const char* name);

/********************Voucher**********************/

OOP_CLASS(Voucher) { OOP_EXTENDS(Service); };

OOP_CTOR(Voucher, State* parent, const char* name);

/********************Topup**********************/

OOP_CLASS(TopUp) { OOP_EXTENDS(Service); };

OOP_CTOR(TopUp, State* parent, const char* name);

#endif