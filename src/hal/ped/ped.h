#ifndef PED_H_
#define PED_H_

#include "oop.h"
#include <stdbool.h>
#include "event.h"

#define PIN_MAX_LEN            4
#define PIN_MIN_LEN            4
#define PED_PIN_ENTRY_TIME_OUT SECS(10)

typedef enum PedErr_t { PED_ERR_OK, PED_ERR_INPUT } PedErr_t;

typedef enum PedKeyType_t {
    PED_MASTER_KEY,
    PED_PIN_KEY,
    PED_DATA_KEY,
    PED_MAC_KEY
} PedKeyType_t;

typedef enum PedKeyEv_t {
    PED_KEY_EV_DIGIT = 0,
    PED_KEY_EV_ENTER,
    PED_KEY_EV_CANCEL,
    PED_KEY_EV_CLEAR,
    PED_KEY_EV_NONE
} PedKeyEv_t;

OOP_DECLARE_CLASS(Ped)

OOP_VTABLE(Ped) {
    OOP_IMETHOD(void, Ped, init);
    OOP_IMETHOD(PedErr_t, Ped, injectKey, PedKeyType_t, uint8_t*, size_t);
    OOP_IMETHOD(PedErr_t, Ped, enterPinEntryMode);
    OOP_IMETHOD(PedErr_t, Ped, exitPinEntryMode);
    OOP_IMETHOD(PedErr_t, Ped, getPinBlock, char* pan, char* out, size_t len);
    OOP_IMETHOD(PedErr_t, Ped, getMac, size_t keyLen, uint8_t* in, size_t inLen,
                uint8_t* out);
    OOP_IMETHOD(PedErr_t, Ped, encryptAccountData, void* buffer, int bufLen,
                void* decryptedData);
    OOP_IMETHOD(PedKeyEv_t, Ped, poll);
};

OOP_CLASS(Ped) {
    OOP_IMPLEMENTS(Ped);
    OOP_METHOD(PedErr_t, enterPinEntryMode);
    OOP_METHOD(PedErr_t, exitPinEntryMode);
    OOP_METHOD(PedErr_t, injectMasterKey, uint8_t*, size_t);
    OOP_METHOD(PedErr_t, injectDataKey, uint8_t*, size_t);
    OOP_METHOD(PedErr_t, injectPinKey, uint8_t*, size_t);
    OOP_METHOD(PedErr_t, injectMacKey, uint8_t*, size_t);
    OOP_METHOD(PedErr_t, getMac, size_t keyLen, uint8_t* in, size_t inLen,
               uint8_t* out);
};

OOP_CTOR(Ped);

Ped* ped(void);

#define PED_INIT() OOP_CALL(ped(), init)

#endif