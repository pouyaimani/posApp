#include "cellular.h"
#include "dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"
#include "timer.h"

Cellular *__cellular;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/cell_t3Rtos.h"

static void constructT3Rtos() {
    static CellT3Rtos obj;
    __cellular = (Cellular *)&obj;
    OOP_CALL_CTOR(Cellular, __cellular);
    OOP_CALL_CTOR(CellT3Rtos, &obj);
}

#endif
static uint32_t tick;
static void checkCellLoginResult() {
    CellPPPStatus_t st = OOP_CALL(__cellular, getPPPstatus);
    LOG_DEBUG("PPP status = %d ", st);
    if (st != CELL_PPP_DIALING) {
        CellEvent *ev = (CellEvent*)createEvent(SM_EVENT_CELLULAR);
        ev->pppSt = st;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkCellLoginResult);
    }
    if (GET_TICK() - tick >= 30000) {
        CellEvent *ev = (CellEvent*)createEvent(SM_EVENT_CELLULAR);
        ev->pppSt = CELL_PPP_FAILURE;
        DISPATCH_EVENT(ev);
        getEventloop()->unregisterChecker(checkCellLoginResult);
    }
}

static CellErr_t startPPPlogin(const char *apn, const char *user, const char *pass, const char *dialnum) {
    getEventloop()->registerChecker(checkCellLoginResult);
    OOP_CALL(__cellular, startPPPlogin, apn, user, pass, dialnum);
    tick = GET_TICK();
}

OOP_CTOR(Cellular) {
    self->vtable.getNetType = NULL;
    self->vtable.getPPPstatus = NULL;
    self->vtable.getSignalStrength = NULL;
    self->vtable.init = NULL;
    self->vtable.startPPPlogin = NULL;
    self->vtable.ussdGetCharset = NULL;
    self->vtable.ussdInit = NULL;
    self->vtable.ussdRec = NULL;
    self->vtable.ussdSend = NULL;
    self->vtable.ussdStop = NULL;
    self->vtable.getSimStatus = NULL;

    self->startPPPlogin = startPPPlogin;
}

Cellular *getCell() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce cellular is undefined. Make sure correct device is chosen and its cellular driver is developed.
#endif
    );
    return __cellular;
}