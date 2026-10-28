#ifndef PICC_H_
#define PICC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "oop.h"
#include "error.h"
#include "event.h"
#include "piccEvent.h"

// #define EMV_LOG_ENABLE 1

#ifdef PICC_LOG_ENABLE
#define PICC_LOG(...) LOG_TRACE(__VA_ARGS__)
#else
#define PICC_LOG(...) ;
#endif

typedef uint32_t PiccTechMask_t;
enum {
    PICC_TECH_A      = (1u << 0),
    PICC_TECH_B      = (1u << 1),
    PICC_TECH_FELICA = (1u << 2)
};

typedef enum {
    PICC_REMOVE_HALT = 0,
    PICC_REMOVE_WAIT,
    PICC_REMOVE_EMV
} PiccRemoveMode_t;

typedef enum {
    PICC_ERR_OK = 0,
    PICC_ERR_BAD_PARAMETER,
    PICC_ERR_NOT_INITIALIZED,
    PICC_ERR_TIMEOUT,
    PICC_ERR_IO,
    PICC_ERR_NOT_SUPPORTED,
    PICC_ERR_OTHER
} PiccErr_t;

/** Existing APDU API retained; SDK conversion is still unimplemented. */
typedef struct ApduSend_t {
    uint8_t*  command;
    uint32_t* lc;
    uint8_t*  dataIn;
    uint32_t* le;
} ApduReq_t;

typedef struct ApduResp_t {
    uint32_t* lenOut; /* IN: dataOut capacity; OUT: actual response length. */
    uint8_t*  dataOut;
    uint32_t* swa;
    uint32_t* swb;
} ApduResp_t;

OOP_DECLARE_CLASS(Picc)

OOP_VTABLE(Picc) {
    OOP_IMETHOD(PiccErr_t, Picc, init);
    OOP_IMETHOD(PiccErr_t, Picc, close);
    OOP_IMETHOD(PiccErr_t, Picc, powerOn, PiccTechMask_t technologies);
    OOP_IMETHOD(PiccCardType_t, Picc, detect);
    OOP_IMETHOD(PiccErr_t, Picc, activate);
    OOP_IMETHOD(PiccErr_t, Picc, exchangeApdu, const ApduReq_t* request,
                ApduResp_t* response);
    OOP_IMETHOD(PiccErr_t, Picc, remove, PiccRemoveMode_t mode);
    OOP_IMETHOD(bool, Picc, isPresent);
    OOP_IMETHOD(PiccCardType_t, Picc, getCardType);
};

OOP_CLASS(Picc) {
    OOP_IMPLEMENTS(Picc);
    OOP_METHOD(void,
               ioRead);  /**< Called by the existing event-loop integration. */
    PiccData_t data;     /**< Current HAL detection metadata. */
    PiccErr_t  error;    /**< Most recent HAL operation error. */
    bool       opened;   /**< SDK device is open. */
    bool       powered;  /**< RF has been powered successfully. */
    bool eventReported;  /**< Current card/error level already dispatched. */
    bool pollingEnabled; /**< Gate detect() calls during APDU ownership handoff.
                          */
    uint32_t
        scanId; /**< Incremented when an explicit polling session starts. */
};

OOP_CTOR(Picc);
Picc* picc(void);

/** Begin a new event-producing scan after init/powerOn, on the event-loop
 * thread. No synchronous event is dispatched. Wraparound skips zero. */
PiccErr_t piccStartPolling(Picc* self);

/** Suspend detect/event production without closing RF; safe with NULL. */
void piccStopPolling(Picc* self);

#define PICC_INIT()         OOP_CALL(picc(), init)
#define PICC_CLOSE()        OOP_CALL(picc(), close)
#define PICC_POWER_ON(type) OOP_CALL(picc(), powerOn, type)
#define PICC_DETECT()       OOP_CALL(picc(), detect)
#define PICC_REMOVE(mode)   OOP_CALL(picc(), remove, mode)

#endif
