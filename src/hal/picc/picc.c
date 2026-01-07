#include "picc.h"
#include "../dev/dev.h"

Picc *__picc;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/picc_t3Rtos.h"

static void constructT3Rtos() {
    static PiccT3Rtos obj;
    __picc = (Picc *)&obj;
    OOP_CALL_CTOR(Picc, __picc);
    OOP_CALL_CTOR(PiccT3Rtos, &obj);
}

#endif

static bool isDetected(Picc* self) {
    return self->vtable.detect(self) != PICC_CARD_NO_CARD;
}

OOP_CTOR(Picc) {
    self->vtable.isDetected = isDetected;
}

Picc *getPicc() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    constructT3Rtos();
#else
#error Deivce PICC is undefined. Make sure correct device is chosen and its PICC driver is developed.
#endif
    );
    return __picc;
}