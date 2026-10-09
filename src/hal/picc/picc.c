#include "picc.h"

#include "eventloop.h"
#include "logger.h"

static Picc* __picc = NULL;

#ifdef DEVICE_TRENDITT3RTOS

#include "t3Rtos/picc_t3Rtos.h"

static void constructT3Rtos(void) {
    static PiccT3Rtos obj;

    __picc = (Picc*)&obj;

    OOP_CALL_CTOR(Picc, __picc);
    OOP_CALL_CTOR(PiccT3Rtos, &obj);
}

#endif

static bool isPresent(Picc* self) {
    if (self == NULL)
        return false;

    return self->data.cardType != PICC_CARD_NONE &&
           self->data.cardType != PICC_CARD_UNKNOWN;
}

static PiccCardType_t getCardType(Picc* self) {
    if (self == NULL)
        return PICC_CARD_UNKNOWN;

    return self->data.cardType;
}

/*
 * Polling entry point for your event loop.
 *
 * Important difference from magnetic swipe:
 *
 * A magnetic swipe is inherently edge-like. PICC detection is level-like:
 * sdkPiccDetect() can keep returning "card present" while the same card
 * remains on the antenna.
 *
 * Therefore we only dispatch once until the card disappears.
 */
static void readIo(void) {
    Picc* self = __picc;

    if (self == NULL)
        return;

    PiccCardType_t card = OOP_CALL(self, detect);

    if (card == PICC_CARD_NONE) {
        self->eventReported = false;
        return;
    }

    if (card == PICC_CARD_UNKNOWN)
        return;

    if (self->eventReported)
        return;

    self->eventReported = true;

    PiccEvent* ev = (PiccEvent*)createEvent(SM_EVENT_PICC);
    ev->data      = &self->data;
    DISPATCH_EVENT(ev);
}

OOP_CTOR(Picc) {
    self->vtable.isPresent   = isPresent;
    self->vtable.getCardType = getCardType;

    self->ioRead = readIo;

    self->data.cardType = PICC_CARD_NONE;

    self->error         = PICC_ERR_OK;
    self->opened        = false;
    self->powered       = false;
    self->eventReported = false;

    piccEventRegister();
}

Picc* picc(void) {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Device PICC is undefined. Make sure the correct device is selected and its PICC driver is implemented.
#endif
    );

    return __picc;
}