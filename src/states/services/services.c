#include "services.h"
#include "dev/dev.h"
#include "../states.h"

static Service *services[MAX_SERVICE_NUM];
static Service *parent;

Service *getService(ServiceId_t id) {
    CALL_ONCE(
        createServices();
    );
    return services[id];
}

OOP_CTOR(Service, State *parent, const char *name) {
    OOP_CALL_CTOR(parent, name);
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
}