#ifndef SERVICES_H_
#define SERVICES_H_

#include "state.h"

#define MAX_SERVICE_NUM 10

extern int serviceCount;

/*********************Service*********************/

OOP_CLASS(Service) {
    State state;
};

OOP_CTOR(Service, State *parent, const char *name);

typedef enum ServiceId_t {
    SERVICE_ID_SALE = 0,
    SERVICE_ID_BALANCE,
    SERVICE_ID_BILL,
    SERVICE_ID_PAY,
    SERVICE_ID_SIM_CHARGE,
    SERVICE_ID_ALL
} ServiceId_t;

Service *getService(ServiceId_t id);

/*********************Sale*********************/

OOP_CLASS(Sale)
{
    OOP_EXTENDS(Service);
};

OOP_CTOR(Sale, State *parent, const char *name);

/**********************Balance**********************/

OOP_CLASS(Balance)
{
    OOP_EXTENDS(Service);
};

OOP_CTOR(Balance, State *parent, const char *name);

/********************Bill**********************/

OOP_CLASS(Bill)
{
    OOP_EXTENDS(Service);
};

OOP_CTOR(Bill, State *parent, const char *name);

/********************Pay**********************/

OOP_CLASS(Pay)
{
    OOP_EXTENDS(Service);
};

OOP_CTOR(Pay, State *parent, const char *name);

/********************SimCharge**********************/

OOP_CLASS(SimCharge)
{
    OOP_EXTENDS(Service);
};

OOP_CTOR(SimCharge, State *parent, const char *name);

#endif