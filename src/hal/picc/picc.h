#ifndef PICC_H_
#define PICC_H_

#include "oop.h"
#include <stdbool.h>
#include <stdint.h>
#include "dcfg.h"

typedef enum PiccErr_t {
    PICC_ERR_OK,
    PICC_ERR_INPUT,
    PICC_ERR_TIME_OUT
} PiccErr_t;

typedef enum PiccCardType_t {
    PICC_CARD_MORE,
    PICC_CARD_A,
    PICC_CARD_B,
    PICC_CARD_A_MEMORY,
    PICC_CARD_B_MEMORY,
    PICC_CARD_FELICA,
    PICC_CARD_NO_CARD
} PiccCardType_t;

typedef struct ApduSend_t {
    uint8_t *command;
    uint32_t *lc;            // Length of data in
    uint8_t *dataIn;
    uint32_t *le;            // Expected to return the length of the data, the actual length of the returned data.
} ApduSend_t;

typedef struct ApduResp_t {
    uint32_t *lenOut;
    uint8_t *dataOut;
    uint32_t *swa;
    uint32_t *swb;
} ApduResp_t;

OOP_DECLARE_CLASS(Picc)

OOP_VTABLE(Picc) {
    OOP_IMETHOD(void, Picc, init);
    OOP_IMETHOD(PiccCardType_t, Picc, detect);
    OOP_IMETHOD(bool, Picc, isDetected);
    OOP_IMETHOD(PiccErr_t, Picc, exchangeApdu);
    OOP_IMETHOD(ApduSend_t*, Picc, getSendAdpu);
    OOP_IMETHOD(ApduResp_t*, Picc, getRespAdpu);
};

OOP_CLASS(Picc) {
    OOP_IMPLEMENTS(Picc);
    ApduSend_t sendAdpu;
    ApduResp_t respAdpu;
};

OOP_CTOR(Picc);

Picc *picc(void);

#endif