#include "picc.h"
#include "eventloop.h"
#include "logger.h"

static Picc* instance; /**< Device singleton, constructed once by picc(). */

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/picc_t3Rtos.h"

/** Construct the supplied platform implementation. */
static void constructT3Rtos(void) {
    static PiccT3Rtos obj;
    instance = (Picc*)&obj;
    OOP_CALL_CTOR(Picc, instance);
    OOP_CALL_CTOR(PiccT3Rtos, &obj);
}
#endif

/** Report a detected field occupant (MULTIPLE also means occupied). */
static bool isPresent(Picc* self) {
    return self && self->opened && self->powered &&
           self->data.cardType != PICC_CARD_NONE &&
           self->data.cardType != PICC_CARD_UNKNOWN;
}

/** Get the last detected type without another hardware operation. */
static PiccCardType_t getCardType(Picc* self) {
    return self ? self->data.cardType : PICC_CARD_UNKNOWN;
}

/** Start event production for a new acquisition generation. */
PiccErr_t piccStartPolling(Picc* self) {
    if (!self)
        return PICC_ERR_BAD_PARAMETER;
    if (!self->opened || !self->powered)
        return PICC_ERR_NOT_INITIALIZED;
    ++self->scanId;
    if (self->scanId == 0)
        ++self->scanId;
    self->data.cardType  = PICC_CARD_NONE;
    self->error          = PICC_ERR_OK;
    self->eventReported  = false;
    self->pollingEnabled = true;
    getEventloop()->registerChecker(self->ioRead);
    return PICC_ERR_OK;
}

/** Pause polling; deliberately leave RF and the selected card active. */
void piccStopPolling(Picc* self) {
    if (!self)
        return;
    getEventloop()->registerChecker(self->ioRead);
    self->pollingEnabled = false;
}

/**
 * Event-loop polling, serialized with manager callbacks and SDK operations.
 * A fresh snapshot is published only when the detected type/error changes or
 * when the previous dispatch allocation failed. A no-card event is published
 * once initially and after a card/error so the UI can restore its ordinary
 * prompt.
 */
static void readIo(void) {
    Picc* self = instance;
    if (!self || !self->opened || !self->powered || !self->pollingEnabled)
        return;

    PiccCardType_t previous      = self->data.cardType;
    PiccErr_t      previousError = self->error;
    PiccCardType_t card          = OOP_CALL(self, detect);
    if (card != previous || self->error != previousError)
        self->eventReported = false;

    if (self->eventReported)
        return;

    PiccEvent* ev = (PiccEvent*)createEvent(SM_EVENT_PICC);
    if (!ev) {
        LOG_TRACE("PICC: event allocation failed");
        return;
    }
    ev->snapshot        = self->data;
    ev->data            = &ev->snapshot;
    ev->scanId          = self->scanId;
    ev->error           = (int)self->error;
    self->eventReported = true;
    DISPATCH_EVENT(ev);
}

/** Install base methods and initialize event-session state. */
OOP_CTOR(Picc) {
    self->vtable.isPresent   = isPresent;
    self->vtable.getCardType = getCardType;
    self->ioRead             = readIo;
    self->data.cardType      = PICC_CARD_NONE;
    self->error              = PICC_ERR_OK;
    self->opened             = false;
    self->powered            = false;
    self->eventReported      = false;
    self->pollingEnabled     = false;
    self->scanId             = 0;
    piccEventRegister();
}

/** Return the platform-selected singleton. */
Picc* picc(void) {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Device PICC is undefined. Select a supported PICC driver.
#endif
    );
    return instance;
}
