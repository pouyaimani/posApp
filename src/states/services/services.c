#include "services.h"
#include "dev/dev.h"
#include "../states.h"
#include "storage/storage.h"
#include "settings/settings.h"
#include "txn.h"
#include "logger.h"

static Service *services[MAX_SERVICE_NUM];
static Service *parent;

static const char *TxnTypeStr[] = {
    [TXN_SALE]            = "SALE",
    [TXN_BILL]            = "BILL",
    [TXN_TOPUP]           = "TOPUP",
    [TXN_BALANCE]         = "BALANCE",
    [TXN_PAY]             = "PAY",
    [TXN_SIM_CHARGE_CODE] = "SIM_CHARGE_CODE",
    [TXN_ALL]             = "ALL"
};

int8_t getTxnName(TxnType type, char *name, size_t size) {
    RETURN_VALUE_IF_NOT((type > TXN_ALL), false, ; ,ERR_BAD_PARAMETER);
    snprintf(name, size, "%s", TxnTypeStr[type]);
    return ERR_OK;
}

static void createServices() {
    parent = (Service*)GET_MEM(sizeof(Service));
    services[SERVICE_ID_SALE] = (Sale*)GET_MEM(sizeof(Sale));
    services[SERVICE_ID_BILL] = (Bill*)GET_MEM(sizeof(Bill));
    services[SERVICE_ID_BALANCE] = (Balance*)GET_MEM(sizeof(Balance));
    services[SERVICE_ID_PAY] = (Pay*)GET_MEM(sizeof(Pay));
    services[SERVICE_ID_SIM_CHARGE] = (SimCharge*)GET_MEM(sizeof(SimCharge));

    OOP_CALL_CTOR(Service, parent, getState(STATE_ID_CARD_HOLDER), "parent");
    OOP_CALL_CTOR(Sale, services[SERVICE_ID_SALE], parent, "خرید");
    OOP_CALL_CTOR(Bill, services[SERVICE_ID_BILL], parent, "پرداخت قبض");
    OOP_CALL_CTOR(Balance, services[SERVICE_ID_BALANCE],parent, "موجودی");
    OOP_CALL_CTOR(Pay, services[SERVICE_ID_PAY],parent, "پرداخت");
    OOP_CALL_CTOR(SimCharge, services[SERVICE_ID_SIM_CHARGE], parent, "خرید شارژ");

    for (uint8_t i = 0; i < SERVICE_ID_ALL ; i++) {
        services[i]->enable = settings()->terminal.serviceEn[i];
    }
}

Service *getService(ServiceId_t id) {
    CALL_ONCE(
        createServices();
    );
    return services[id];
}

OOP_CTOR(Service, State *parent, const char *name) {
    OOP_CALL_CTOR(State, self, parent, name);
}