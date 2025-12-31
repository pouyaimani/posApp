#include "picc.h"
#include "../dev/dev.h"

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/picc_t3Rtos.h"
#endif

static Picc *picc;

static bool isDetected(Picc* self) {
    return self->vtable.detect(self) != PICC_CARD_NO_CARD;
}

void Picc_ctor(Picc* self) {
    self->vtable.isDetected = isDetected;
}

Picc *getPicc() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
    DEVICE_REGISTER(Picc, PiccT3Rtos, picc)
#else
#error Deivce PICC is undefined. Make sure correct device is chosen and its PICC driver is developed.
#endif
    );
    return picc;
}