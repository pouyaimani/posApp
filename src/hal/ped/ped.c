#include "ped.h"

#include "event.h"
#include "eventloop.h"
#include "logger.h"

Ped* __ped;

#ifdef DEVICE_TRENDITT3RTOS
#include "t3Rtos/ped_t3Rtos.h"

static void constructT3Rtos() {
    static PedT3Rtos obj;
    __ped = (Ped*)&obj;
    OOP_CALL_CTOR(Ped, __ped);
    OOP_CALL_CTOR(PedT3Rtos, &obj);
}

#endif

static PedErr_t injectMasterKey(uint8_t* key, size_t len) {
    return OOP_CALL(__ped, injectKey, PED_MASTER_KEY, key, len);
}

static PedErr_t injectDataKey(uint8_t* key, size_t len) {
    return OOP_CALL(__ped, injectKey, PED_DATA_KEY, key, len);
}

static PedErr_t injectPinKey(uint8_t* key, size_t len) {
    return OOP_CALL(__ped, injectKey, PED_PIN_KEY, key, len);
}

static PedErr_t injectMacKey(uint8_t* key, size_t len) {
    return OOP_CALL(__ped, injectKey, PED_MAC_KEY, key, len);
}

PedErr_t getMac(size_t keyLen, uint8_t* in, size_t inLen, void* out) {
    return OOP_CALL(__ped, getMac, keyLen, in, inLen, out);
}

OOP_CTOR(Ped) {
    LOG_TRACE("Constructing ped ...");
    self->vtable.init              = NULL;
    self->vtable.injectKey         = NULL;
    self->vtable.enterPinEntryMode = NULL;
    self->vtable.exitPinEntryMode  = NULL;
    self->injectMasterKey          = injectMasterKey;
    self->injectPinKey             = injectPinKey;
    self->injectDataKey            = injectDataKey;
    self->injectMacKey             = injectMacKey;
    self->getMac                   = getMac;
}

Ped* ped() {
    CALL_ONCE(
#ifdef DEVICE_TRENDITT3RTOS
        constructT3Rtos();
#else
#error Deivce keypad is undefined. Make sure correct device is chosen and its keypad driver is developed.
#endif
    );
    return __ped;
}