#ifndef PICC_H_
#define PICC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "oop.h"
#include "error.h"
#include "event.h"
#include "piccEvent.h"

/* -----------------------------------------------------------
 * Supported contactless technologies
 * ----------------------------------------------------------- */

typedef uint32_t PiccTechMask_t;

enum {
    PICC_TECH_A      = (1u << 0),
    PICC_TECH_B      = (1u << 1),
    PICC_TECH_FELICA = (1u << 2),
};

/* -----------------------------------------------------------
 * Card removal mode
 * ----------------------------------------------------------- */

typedef enum {
    PICC_REMOVE_HALT = 0,

    /* Deactivate and wait until card is physically removed. */
    PICC_REMOVE_WAIT,

    /* EMV contactless removal behavior. */
    PICC_REMOVE_EMV
} PiccRemoveMode_t;

/* -----------------------------------------------------------
 * Error
 *
 * You can move this to error.h if that is where you keep
 * device error definitions.
 * ----------------------------------------------------------- */

typedef enum {
    PICC_ERR_OK = 0,

    PICC_ERR_BAD_PARAMETER,
    PICC_ERR_NOT_INITIALIZED,
    PICC_ERR_TIMEOUT,
    PICC_ERR_IO,
    PICC_ERR_NOT_SUPPORTED,
    PICC_ERR_OTHER
} PiccErr_t;

/*
 * Platform-neutral APDU representation.
 */
typedef struct ApduSend_t {
    uint8_t*  command;
    uint32_t* lc; // Length of data in
    uint8_t*  dataIn;
    uint32_t* le; // Expected to return the length of the data, the actual
                  // length of the returned data.
} ApduReq_t;

typedef struct ApduResp_t {
    uint32_t* lenOut;
    uint8_t*  dataOut;
    uint32_t* swa;
    uint32_t* swb;
} ApduResp_t;

/* -----------------------------------------------------------
 * PICC class
 * ----------------------------------------------------------- */

OOP_DECLARE_CLASS(Picc)

OOP_VTABLE(Picc) {
    OOP_IMETHOD(PiccErr_t, Picc, init);

    OOP_IMETHOD(PiccErr_t, Picc, close);

    OOP_IMETHOD(PiccErr_t, Picc, powerOn, PiccTechMask_t technologies);

    OOP_IMETHOD(PiccCardType_t, Picc, detect);

    OOP_IMETHOD(PiccErr_t, Picc, exchangeApdu, const ApduReq_t* request,
                ApduResp_t* response);

    OOP_IMETHOD(PiccErr_t, Picc, remove, PiccRemoveMode_t mode);

    OOP_IMETHOD(bool, Picc, isPresent);

    OOP_IMETHOD(PiccCardType_t, Picc, getCardType);
};

OOP_CLASS(Picc) {
    OOP_IMPLEMENTS(Picc);

    /*
     * Called periodically by your IO/event loop.
     */
    OOP_METHOD(void, ioRead);

    PiccData_t data;

    PiccErr_t error;

    bool opened;
    bool powered;

    /*
     * Prevent dispatching the same card-present event
     * continuously while the card stays on the antenna.
     */
    bool eventReported;
};

OOP_CTOR(Picc);

Picc* picc(void);

#define PICC_INIT() OOP_CALL(picc(), init)

#define PICC_CLOSE() OOP_CALL(picc(), close)

#define PICC_POWER_ON(type) OOP_CALL(picc(), powerOn, type)

#define PICC_DETECT() OOP_CALL(picc(), detect)

#define PICC_REMOVE(mode) OOP_CALL(picc(), remove, mode)

#endif