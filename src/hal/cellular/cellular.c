#include "cellular.h"
#include "dev/dev.h"
#include "eventloop.h"
#include "event.h"
#include "logger.h"

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